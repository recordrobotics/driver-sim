#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <SDL3/SDL.h>
#include <bx/file.h>
#include <bx/error.h>
#include <bx/pixelformat.h>
#include <bimg/decode.h>
#include <imgui/imgui.h>

#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <random>

#include <nlohmann/json.hpp>

#include <blackboard_app/logger.h>

#include <cassert>

#include "texture.h"

#include "fieldrenderer.h"
#include "shaders.h"
#include "mesh.h"
#include <future>

#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>
#include <networktables/StructTopic.h>
#include <networktables/StructArrayTopic.h>

#include <wpi/struct/Struct.h>

#include <frc/geometry/Pose3d.h>
#include <frc/geometry/struct/Pose3dStruct.h>

#include "../settings/settingsstore.h"

#include <bloom_dirt_mask.png.h>
#include <lut.exr.h>

#include <carpet_base_color.jpg.h>
#include <carpet_bump.jpg.h>

#if GAME_YEAR == 2026
#include "seasonspecific/rebuilt2026/fmsui.h"
#include "seasonspecific/rebuilt2026/hublights.h"
#endif

struct Pose3dObject
{
    frc::Pose3d pose;
    int identity;

    Pose3dObject(const frc::Pose3d &p, int id) : pose(p), identity(id) {}

    Pose3dObject() : pose(frc::Pose3d()), identity(0) {}
};

namespace
{
    constexpr size_t kPoseOff = 0;
    constexpr size_t kIdentityOff =
        kPoseOff + wpi::GetStructSize<frc::Pose3d>();
} // namespace

template <>
struct wpi::Struct<Pose3dObject>
{
    static constexpr std::string_view GetTypeName() { return "Pose3dObject"; }
    static constexpr size_t GetSize()
    {
        return wpi::GetStructSize<frc::Pose3d>() + 4;
    }
    static constexpr std::string_view GetSchema()
    {
        return "Pose3d pose;int identity";
    }

    static Pose3dObject Unpack(std::span<const uint8_t> data)
    {
        return Pose3dObject{
            wpi::UnpackStruct<frc::Pose3d, kPoseOff>(data),
            wpi::UnpackStruct<int, kIdentityOff>(data)};
    }
    static void Pack(std::span<uint8_t> data, const Pose3dObject &value)
    {
        wpi::PackStruct<kPoseOff>(data, value.pose);
        wpi::PackStruct<kIdentityOff>(data, value.identity);
    }
    static void ForEachNested(
        std::invocable<std::string_view, std::string_view> auto fn)
    {
        wpi::ForEachStructSchema<frc::Pose3d>(fn);
    }
};

static_assert(wpi::StructSerializable<Pose3dObject>);
static_assert(wpi::HasNestedStruct<Pose3dObject>);

static const bgfx::EmbeddedShader s_embeddedShaders[] =
    {
        BGFX_EMBEDDED_SHADER(vs_pbr),
        BGFX_EMBEDDED_SHADER(vs_pbr_instanced),
        BGFX_EMBEDDED_SHADER(fs_pbr),
        BGFX_EMBEDDED_SHADER(fs_pbr_textured),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit_depth_post_pass),

        BGFX_EMBEDDED_SHADER(vs_pass),
        BGFX_EMBEDDED_SHADER(fs_tonemap),

        BGFX_EMBEDDED_SHADER(cs_blit),
        BGFX_EMBEDDED_SHADER(cs_debug_normals),
        BGFX_EMBEDDED_SHADER(cs_exposure),
        BGFX_EMBEDDED_SHADER(cs_oit_comp),
        BGFX_EMBEDDED_SHADER(cs_taa_resolve),
        BGFX_EMBEDDED_SHADER(cs_mb_velocity),
        BGFX_EMBEDDED_SHADER(cs_mb_guertin_tile_max_x),
        BGFX_EMBEDDED_SHADER(cs_mb_guertin_tile_max_y),
        BGFX_EMBEDDED_SHADER(cs_mb_guertin_neighbor_max),
        BGFX_EMBEDDED_SHADER(cs_mb_guertin_tile_variance),
        BGFX_EMBEDDED_SHADER(cs_mb_guertin_experimental_blur),

        BGFX_EMBEDDED_SHADER(cs_bloom_downscale),
        BGFX_EMBEDDED_SHADER(cs_bloom_upscale),

        BGFX_EMBEDDED_SHADER(cs_XeGTAO_PrefilterDepths16x16),
        BGFX_EMBEDDED_SHADER(cs_XeGTAO_MainPass),
        BGFX_EMBEDDED_SHADER(cs_XeGTAO_debugNormals),
        BGFX_EMBEDDED_SHADER(cs_XeGTAO_debugVisibility),

        BGFX_EMBEDDED_SHADER_END()};

enum class ModelRotationAxis
{
    X,
    Y,
    Z
};

typedef struct ModelRotationConfig
{
    ModelRotationAxis axis;
    float angleDegrees;
} ModelRotationConfig;

static ModelRotationAxis axisFromString(const std::string &s)
{
    std::string sLower = s;
    std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

    if (sLower == "x")
        return ModelRotationAxis::X;
    if (sLower == "y")
        return ModelRotationAxis::Y;
    if (sLower == "z")
        return ModelRotationAxis::Z;
    throw std::runtime_error("Invalid rotation axis in config: " + s);
}

namespace nlohmann
{
    template <>
    struct adl_serializer<ModelRotationConfig>
    {
        static ModelRotationConfig from_json(const json &j)
        {
            return {
                axisFromString(j.at("axis").get<std::string>()),
                j.at("degrees").get<float>()};
        }
    };
}

enum class WPILibCoordinateSystem
{
    WallBlue,
    CenterRed
};

static WPILibCoordinateSystem coordinateSystemFromString(const std::string &s)
{
    std::string sLower = s;
    std::transform(sLower.begin(), sLower.end(), sLower.begin(), ::tolower);

    if (sLower == "wall-blue")
        return WPILibCoordinateSystem::WallBlue;
    if (sLower == "center-red")
        return WPILibCoordinateSystem::CenterRed;
    throw std::runtime_error("Invalid coordinate system in config: " + s);
}

enum class CameraView
{
    Field,
    Robot,
    RobotRelative,
    Count
};

static const std::array<std::string, static_cast<size_t>(CameraView::Count)> CAMERA_VIEW_NAMES = {
    "Field",
    "Robot",
    "Robot Relative"};

enum class DebugView
{
    None,
    Albedo,
    Normal,
    Emission,
    PBRData,
    Velocity,
    Depth,
    OITAccum,
    OITReveal,
    MotionBlurVelocity,
    MotionBlurTileMaxX,
    MotionBlurTileMaxY,
    MotionBlurNeighborMax,
    MotionBlurTileVariance,
    GTAOWorkingDepth0,
    GTAOWorkingDepth1,
    GTAOWorkingDepth2,
    GTAOWorkingDepth3,
    GTAOWorkingDepth4,
    GTAOWorkingAOTermNormals,
    GTAOWorkingAOTermVisibility,
    GTAOWorkingEdges,
    Count
};

static const std::array<std::string, static_cast<size_t>(DebugView::Count)> DEBUG_VIEW_NAMES = {
    "None",
    "Albedo",
    "Normal",
    "Emission",
    "PBR Data",
    "Velocity",
    "Depth",
    "OIT Accum",
    "OIT Reveal",
    "Motion Blur Velocity",
    "Motion Blur Tile Max X",
    "Motion Blur Tile Max Y",
    "Motion Blur Neighbor Max",
    "Motion Blur Tile Variance",
    "GTAO Working Depth 0",
    "GTAO Working Depth 1",
    "GTAO Working Depth 2",
    "GTAO Working Depth 3",
    "GTAO Working Depth 4",
    "GTAO Working AO Term Normals",
    "GTAO Working AO Term Visibility",
    "GTAO Working Edges"};

struct Vec3Padded
{
    bx::Vec3 vec;
    float padding; // padding to make it vec4 that bgfx expects

    Vec3Padded() : vec(0.0f, 0.0f, 0.0f), padding(0.0f) {}
    Vec3Padded(float x, float y, float z) : vec(x, y, z), padding(0.0f) {}
    Vec3Padded(const bx::Vec3 &v) : vec(v), padding(0.0f) {}
};

static constexpr float INCHES_TO_METERS = 0.0254f;

uint8_t MB_TILE_SIZE = 40;
uint8_t MB_SAMPLE_COUNT = 32;
float MB_MAXIMUM_JITTER_VALUE = 0.95f;
bool MB_FRAMERATE_INDEPENDENT = true;
bool MB_UNCAPPED_INDEPENDENCE = true;
float MB_TARGET_CONSTANT_FRAMERATE = 30;

static constexpr uint8_t BLOOM_DOWNSCALE_LIMIT = 10;
static constexpr uint8_t BLOOM_MAX_ITERATIONS = 16;

static uint8_t calculateBloomMipmapLevels(uint16_t width, uint16_t height)
{
    width /= 2;
    height /= 2;
    uint8_t mip_levels = 1;

    for (uint8_t i = 0; i < BLOOM_MAX_ITERATIONS; ++i)
    {
        width /= 2;
        height /= 2;

        if (width < BLOOM_DOWNSCALE_LIMIT || height < BLOOM_DOWNSCALE_LIMIT)
            break;

        ++mip_levels;
    }

    return mip_levels + 1;
}

typedef struct MBVelocityComponent
{
    float multiplier = 1.0f;
    float lowerThreshold = 0.0f;
    float upperThreshold = 0.0f;
} MBVelocityComponent;

static constexpr MBVelocityComponent MB_CAMERA_ROTATION_COMPONENT = {1.0f, 0.0f, 1.0f};
static constexpr MBVelocityComponent MB_CAMERA_MOVEMENT_COMPONENT = {1.0f, 0.0f, 1.0f};
static constexpr MBVelocityComponent MB_OBJECT_MOVEMENT_COMPONENT = {20.0f, 0.0f, 10.0f};

using namespace blackboard::logger;

static constexpr uint16_t VIEW_GBUFFER = 0;
static constexpr uint16_t VIEW_GTAO = 1;
static constexpr uint16_t VIEW_OIT = 2;
static constexpr uint16_t VIEW_OIT_DEPTH_POST_PASS = 3;
static constexpr uint16_t VIEW_POSTPROCESS = 4;
static constexpr uint16_t VIEW_BLIT = 5;

static constexpr uint8_t LIGHT_COUNT = 6;

static constexpr uint8_t XE_GTAO_DEPTH_MIP_LEVELS = 5;

// from https://github.com/Unity-Technologies/Graphics/blob/master/com.unity.postprocessing/PostProcessing/Shaders/Colors.hlsl
inline float PositivePow(float base, float power)
{
    return std::pow(std::max(std::abs(base), 1.192092896e-07f), power);
}

inline std::array<float, 4> SRGBToLinear(const std::array<float, 4> &srgb)
{
    std::array<float, 4> linear;
    for (size_t i = 0; i < 3; ++i)
    {
        if (srgb[i] <= 0.04045f)
            linear[i] = srgb[i] / 12.92f;
        else
            linear[i] = PositivePow((srgb[i] + 0.055f) / 1.055f, 2.4f);
    }
    linear[3] = srgb[3]; // Alpha channel remains unchanged
    return linear;
}

bgfx::UniformHandle u_baseColor;
bgfx::UniformHandle u_emissionColor;
bgfx::UniformHandle u_skyColor;
bgfx::UniformHandle u_info;
bgfx::UniformHandle u_previousModelViewProj;
bgfx::UniformHandle u_previousView;
bgfx::UniformHandle u_previousProj;
bgfx::UniformHandle u_jitter;
bgfx::UniformHandle u_pbrData;

bgfx::UniformHandle u_lightPos;
bgfx::UniformHandle u_lightColor;

bgfx::UniformHandle u_mbVelocityData;
bgfx::UniformHandle u_mbBlurData;

bgfx::UniformHandle u_bloomThreshold;
bgfx::UniformHandle u_bloomTexelSize;
bgfx::UniformHandle u_bloomIntensity;

bgfx::UniformHandle u_lutParams;

bgfx::UniformHandle u_XeGTAOData;

bgfx::ProgramHandle programPBR = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programPBRTextured = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programPBRInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOit = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitDepthPostPass = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitDepthPostPassInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle oitCompProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle tonemapProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle exposureProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle blitProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle debugNormalsProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle taaResolveProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle mbVelocityProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbGuertinTileMaxXProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbGuertinTileMaxYProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbGuertinNeighborMaxProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbGuertinTileVarianceProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbGuertinExperimentalBlurProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle bloomDownscaleProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle bloomUpscaleProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle XeGTAO_PrefilterDepths16x16Program = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle XeGTAO_MainPassProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle XeGTAO_debugNormalsProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle XeGTAO_debugVisibilityProgram = BGFX_INVALID_HANDLE;

Texture gAccumTex;
Texture gRevealTex;

Texture gbufAlbedo;
Texture gbufEmission;
Texture gbufNormal;
Texture gbufPBRData;
Texture gbufVelocity;
Texture gFullVelocity;
Texture gbufDepth;

Texture gOutputColor;

Texture gTAABuffer0;
Texture gTAABuffer1;

Texture gMBTileMaxX;
Texture gMBTileMax;
Texture gMBNeighborMax;
Texture gMBTileVariance;
Texture gMBVelocity;
Texture gMBOutputColor;

Texture gGTAOWorkingDepth;
Texture gGTAOWorkingAOTerm;
Texture gGTAOWorkingEdges;
Texture gGTAOHilbertLut;

Texture bloomDirtMask;

Texture carpetBaseColor;
Texture carpetBump;

Texture tonemappingLut;

FrameBuffer gBufFbo;
FrameBuffer gOitFbo;
FrameBuffer gOitDepthPostPassFbo;

bgfx::UniformHandle s_tex;
bgfx::UniformHandle s_taaHistory;

bgfx::UniformHandle s_color;
bgfx::UniformHandle s_velocity;
bgfx::UniformHandle s_depth;

bgfx::UniformHandle s_mbTileMaxX;
bgfx::UniformHandle s_mbTileMax;
bgfx::UniformHandle s_mbNeighborMax;
bgfx::UniformHandle s_mbTileVariance;

bgfx::UniformHandle s_bloomDirt;

bgfx::UniformHandle s_lut;

bgfx::UniformHandle s_baseColor;
bgfx::UniformHandle s_bump;

float bloomThreshold = 5.2f;
float bloomKnee = 0.1f;
float bloomIntensity = 1.0f;
float bloomDirtIntensity = 1.1f;

float tonemappingExposure = -2.0f;

float gtaoEffectRadius = 0.5f;
float gtaoRadiusMultiplier = 1.457f;
float gtaoEffectFalloffRange = 0.615f;
float gtaoSampleDistributionPower = 2.0f;
float gtaoThinOccluderCompensation = 0.0f;
float gtaoDepthMipSamplingOffset = 3.30f;
float gtaoFinalValuePower = 2.2f;

float fieldModelMatrix[16];

WPILibCoordinateSystem coordinateSystem = WPILibCoordinateSystem::CenterRed;
float fieldWidthMeters = 1.0f;
float fieldHeightMeters = 1.0f;

std::future<void> fieldModelLoadingFuture;
std::future<void> robotModelLoadingFuture;

nt::NetworkTableInstance ntInst;

#if GAME_YEAR == 2026
std::unique_ptr<Rebuilt2026FMSUI> fmsUI;
#endif

CameraView cameraView = CameraView::Field;
DebugView debugView = DebugView::None;

bool freezeTemporalEffects = false;

void initPBROIT(uint16_t width, uint16_t height)
{
    s_baseColor = bgfx::createUniform("s_baseColor", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_baseColor))
    {
        logger->error("Failed to create uniform for base color texture.");
        throw std::runtime_error("Failed to create uniform for base color texture.");
    }

    s_bump = bgfx::createUniform("s_bump", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_bump))
    {
        logger->error("Failed to create uniform for bump texture.");
        throw std::runtime_error("Failed to create uniform for bump texture.");
    }

    u_baseColor = bgfx::createUniform("u_baseColor", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_baseColor))
    {
        logger->error("Failed to create uniform: u_baseColor");
        throw std::runtime_error("Failed to create uniform: u_baseColor");
    }

    u_emissionColor = bgfx::createUniform("u_emissionColor", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_emissionColor))
    {
        logger->error("Failed to create uniform: u_emissionColor");
        throw std::runtime_error("Failed to create uniform: u_emissionColor");
    }

    u_skyColor = bgfx::createUniform("u_skyColor", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_skyColor))
    {
        logger->error("Failed to create uniform: u_skyColor");
        throw std::runtime_error("Failed to create uniform: u_skyColor");
    }

    u_info = bgfx::createUniform("u_info", bgfx::UniformType::Vec4, 2);

    if (!bgfx::isValid(u_info))
    {
        logger->error("Failed to create uniform: u_info");
        throw std::runtime_error("Failed to create uniform: u_info");
    }

    u_pbrData = bgfx::createUniform("u_pbrData", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_pbrData))
    {
        logger->error("Failed to create uniform: u_pbrData");
        throw std::runtime_error("Failed to create uniform: u_pbrData");
    }

    u_lightPos = bgfx::createUniform("u_lightPos", bgfx::UniformType::Vec4, LIGHT_COUNT);
    if (!bgfx::isValid(u_lightPos))
    {
        logger->error("Failed to create uniform: u_lightPos");
        throw std::runtime_error("Failed to create uniform: u_lightPos");
    }

    u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4, LIGHT_COUNT);
    if (!bgfx::isValid(u_lightColor))
    {
        logger->error("Failed to create uniform: u_lightColor");
        throw std::runtime_error("Failed to create uniform: u_lightColor");
    }

    const auto type = bgfx::getRendererType();

    programPBR =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr"), true);

    if (!bgfx::isValid(programPBR))
    {
        logger->error("Failed to create PBR rendering program.");
        throw std::runtime_error("Failed to create PBR rendering program.");
    }

    programPBRTextured =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_textured"), true);

    if (!bgfx::isValid(programPBRTextured))
    {
        logger->error("Failed to create PBR textured rendering program.");
        throw std::runtime_error("Failed to create PBR textured rendering program.");
    }

    programPBRInstanced =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_instanced"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr"), true);

    if (!bgfx::isValid(programPBRInstanced))
    {
        logger->error("Failed to create PBR instanced rendering program.");
        throw std::runtime_error("Failed to create PBR instanced rendering program.");
    }

    programOit =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit"), true);

    if (!bgfx::isValid(programOit))
    {
        logger->error("Failed to create OIT rendering program.");
        throw std::runtime_error("Failed to create OIT rendering program.");
    }

    programOitInstanced =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_instanced"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit"), true);

    if (!bgfx::isValid(programOitInstanced))
    {
        logger->error("Failed to create OIT instanced rendering program.");
        throw std::runtime_error("Failed to create OIT instanced rendering program.");
    }

    programOitDepthPostPass =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit_depth_post_pass"), true);

    if (!bgfx::isValid(programOitDepthPostPass))
    {
        logger->error("Failed to create OIT depth post-pass program.");
        throw std::runtime_error("Failed to create OIT depth post-pass program.");
    }

    programOitDepthPostPassInstanced =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_instanced"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit_depth_post_pass"), true);

    if (!bgfx::isValid(programOitDepthPostPassInstanced))
    {
        logger->error("Failed to create OIT instanced depth post-pass program.");
        throw std::runtime_error("Failed to create OIT instanced depth post-pass program.");
    }

    oitCompProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_oit_comp"), true);

    if (!bgfx::isValid(oitCompProgram))
    {
        logger->error("Failed to create OIT composition program.");
        throw std::runtime_error("Failed to create OIT composition program.");
    }

    TEXTURE(
        gAccumTex,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gRevealTex,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    FRAMEBUFFER(
        gOitFbo,
        &gAccumTex, &gRevealTex, &gbufDepth);

    FRAMEBUFFER(
        gOitDepthPostPassFbo,
        &gbufVelocity, &gbufDepth);

    TEXTURE(
        gOutputColor,
        width, height,
        1.0f, 1.0f,
        true,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    gOutputColor.mipCount = calculateBloomMipmapLevels(gOutputColor.width, gOutputColor.height);

    TEXTURE_EMBEDDED(
        carpetBaseColor,
        carpet_base_color_jpg,
        bimg::TextureFormat::BGRA8,
        bgfx::TextureFormat::BGRA8,
        0);

    TEXTURE_EMBEDDED(
        carpetBump,
        carpet_bump_jpg,
        bimg::TextureFormat::BGRA8,
        bgfx::TextureFormat::BGRA8,
        0);
}

void initTonemap()
{
    const auto type = bgfx::getRendererType();

    s_lut = bgfx::createUniform("s_lut", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_lut))
    {
        logger->error("Failed to create uniform: s_lut");
        throw std::runtime_error("Failed to create uniform: s_lut");
    }

    u_lutParams = bgfx::createUniform("u_lutParams", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_lutParams))
    {
        logger->error("Failed to create uniform: u_lutParams");
        throw std::runtime_error("Failed to create uniform: u_lutParams");
    }

    exposureProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_exposure"), true);

    if (!bgfx::isValid(exposureProgram))
    {
        logger->error("Failed to create exposure program.");
        throw std::runtime_error("Failed to create exposure program.");
    }

    tonemapProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pass"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_tonemap"), true);

    if (!bgfx::isValid(tonemapProgram))
    {
        logger->error("Failed to create tonemap program.");
        throw std::runtime_error("Failed to create tonemap program.");
    }

    TEXTURE_EMBEDDED(
        tonemappingLut,
        lut_exr,
        bimg::TextureFormat::RGBA32F,
        bgfx::TextureFormat::RGBA32F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
}

void initTAA(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_taaHistory = bgfx::createUniform("s_taaHistory", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_taaHistory))
    {
        logger->error("Failed to create uniform for TAA history texture.");
        throw std::runtime_error("Failed to create uniform for TAA history texture.");
    }

    u_jitter = bgfx::createUniform("u_jitter", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_jitter))
    {
        logger->error("Failed to create uniform: u_jitter");
        throw std::runtime_error("Failed to create uniform: u_jitter");
    }

    taaResolveProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_taa_resolve"), true);

    if (!bgfx::isValid(taaResolveProgram))
    {
        logger->error("Failed to create TAA resolve program.");
        throw std::runtime_error("Failed to create TAA resolve program.");
    }

    TEXTURE(
        gTAABuffer0,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gTAABuffer1,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);
}

void initGBuffer(uint16_t width, uint16_t height)
{
    s_color = bgfx::createUniform("s_color", bgfx::UniformType::Sampler);
    s_velocity = bgfx::createUniform("s_velocity", bgfx::UniformType::Sampler);
    s_depth = bgfx::createUniform("s_depth", bgfx::UniformType::Sampler);

    TEXTURE(
        gbufAlbedo,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gbufEmission,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gbufNormal,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R32U,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gbufPBRData,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gbufVelocity,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gFullVelocity,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gbufDepth,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::D32F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    FRAMEBUFFER(
        gBufFbo,
        &gbufAlbedo, &gbufEmission, &gbufNormal, &gbufPBRData, &gbufVelocity, &gbufDepth);
}

void initMotionBlur(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_mbTileMaxX = bgfx::createUniform("s_tilemax_x", bgfx::UniformType::Sampler);
    s_mbTileMax = bgfx::createUniform("s_tilemax", bgfx::UniformType::Sampler);
    s_mbNeighborMax = bgfx::createUniform("s_neighbormax", bgfx::UniformType::Sampler);
    s_mbTileVariance = bgfx::createUniform("s_tilevariance", bgfx::UniformType::Sampler);

    u_previousModelViewProj = bgfx::createUniform("u_previousModelViewProj", bgfx::UniformType::Mat4);
    if (!bgfx::isValid(u_previousModelViewProj))
    {
        logger->error("Failed to create uniform: u_previousModelViewProj");
        throw std::runtime_error("Failed to create uniform: u_previousModelViewProj");
    }

    u_previousView = bgfx::createUniform("u_previousView", bgfx::UniformType::Mat4);
    if (!bgfx::isValid(u_previousView))
    {
        logger->error("Failed to create uniform: u_previousView");
        throw std::runtime_error("Failed to create uniform: u_previousView");
    }

    u_previousProj = bgfx::createUniform("u_previousProj", bgfx::UniformType::Mat4);
    if (!bgfx::isValid(u_previousProj))
    {
        logger->error("Failed to create uniform: u_previousProj");
        throw std::runtime_error("Failed to create uniform: u_previousProj");
    }

    u_mbVelocityData = bgfx::createUniform("u_mbVelocityData", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_mbVelocityData))
    {
        logger->error("Failed to create uniform: u_mbVelocityData");
        throw std::runtime_error("Failed to create uniform: u_mbVelocityData");
    }

    u_mbBlurData = bgfx::createUniform("u_mbBlurData", bgfx::UniformType::Vec4, 2);
    if (!bgfx::isValid(u_mbBlurData))
    {
        logger->error("Failed to create uniform: u_mbBlurData");
        throw std::runtime_error("Failed to create uniform: u_mbBlurData");
    }

    mbVelocityProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_velocity"), true);
    if (!bgfx::isValid(mbVelocityProgram))
    {
        logger->error("Failed to create motion blur velocity program.");
        throw std::runtime_error("Failed to create motion blur velocity program.");
    }

    mbGuertinTileMaxXProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_guertin_tile_max_x"), true);
    if (!bgfx::isValid(mbGuertinTileMaxXProgram))
    {
        logger->error("Failed to create motion blur tile max X program.");
        throw std::runtime_error("Failed to create motion blur tile max X program.");
    }

    mbGuertinTileMaxYProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_guertin_tile_max_y"), true);
    if (!bgfx::isValid(mbGuertinTileMaxYProgram))
    {
        logger->error("Failed to create motion blur tile max Y program.");
        throw std::runtime_error("Failed to create motion blur tile max Y program.");
    }

    mbGuertinNeighborMaxProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_guertin_neighbor_max"), true);
    if (!bgfx::isValid(mbGuertinNeighborMaxProgram))
    {
        logger->error("Failed to create motion blur neighbor max program.");
        throw std::runtime_error("Failed to create motion blur neighbor max program.");
    }

    mbGuertinTileVarianceProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_guertin_tile_variance"), true);
    if (!bgfx::isValid(mbGuertinTileVarianceProgram))
    {
        logger->error("Failed to create motion blur tile variance program.");
        throw std::runtime_error("Failed to create motion blur tile variance program.");
    }

    mbGuertinExperimentalBlurProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_guertin_experimental_blur"), true);
    if (!bgfx::isValid(mbGuertinExperimentalBlurProgram))
    {
        logger->error("Failed to create motion blur experimental blur program.");
        throw std::runtime_error("Failed to create motion blur experimental blur program.");
    }

    TEXTURE(
        gMBTileMaxX,
        width,
        height,
        1.0f / MB_TILE_SIZE,
        1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBTileMax,
        width,
        height,
        1.0f / MB_TILE_SIZE,
        1.0f / MB_TILE_SIZE,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBNeighborMax,
        width,
        height,
        1.0f / MB_TILE_SIZE,
        1.0f / MB_TILE_SIZE,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBTileVariance,
        width,
        height,
        1.0f / MB_TILE_SIZE,
        1.0f / MB_TILE_SIZE,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBOutputColor,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBVelocity,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);
}

void initBloom(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_bloomDirt = bgfx::createUniform("s_bloomDirt", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_bloomDirt))
    {
        logger->error("Failed to create uniform: s_bloomDirt");
        throw std::runtime_error("Failed to create uniform: s_bloomDirt");
    }

    u_bloomThreshold = bgfx::createUniform("u_threshold", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_bloomThreshold))
    {
        logger->error("Failed to create uniform: u_bloomThreshold");
        throw std::runtime_error("Failed to create uniform: u_bloomThreshold");
    }
    u_bloomTexelSize = bgfx::createUniform("u_texel_size", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_bloomTexelSize))
    {
        logger->error("Failed to create uniform: u_bloomTexelSize");
        throw std::runtime_error("Failed to create uniform: u_bloomTexelSize");
    }
    u_bloomIntensity = bgfx::createUniform("u_bloom_intensity", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_bloomIntensity))
    {
        logger->error("Failed to create uniform: u_bloomIntensity");
        throw std::runtime_error("Failed to create uniform: u_bloomIntensity");
    }

    bloomDownscaleProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_bloom_downscale"), true);
    if (!bgfx::isValid(bloomDownscaleProgram))
    {
        logger->error("Failed to create bloom downscale program.");
        throw std::runtime_error("Failed to create bloom downscale program.");
    }

    bloomUpscaleProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_bloom_upscale"), true);
    if (!bgfx::isValid(bloomUpscaleProgram))
    {
        logger->error("Failed to create bloom upscale program.");
        throw std::runtime_error("Failed to create bloom upscale program.");
    }

    TEXTURE_EMBEDDED(
        bloomDirtMask,
        bloom_dirt_mask_png,
        bimg::TextureFormat::BGRA8,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
}

// From https://www.shadertoy.com/view/3tB3z3 - except we're using R2 here
#define XE_HILBERT_LEVEL    6U
#define XE_HILBERT_WIDTH    ( (1U << XE_HILBERT_LEVEL) )
#define XE_HILBERT_AREA     ( XE_HILBERT_WIDTH * XE_HILBERT_WIDTH )
inline uint32_t HilbertIndex( uint32_t posX, uint32_t posY )
{   
    uint32_t index = 0U;
    for( uint32_t curLevel = XE_HILBERT_WIDTH/2U; curLevel > 0U; curLevel /= 2U )
    {
        uint32_t regionX = ( posX & curLevel ) > 0U;
        uint32_t regionY = ( posY & curLevel ) > 0U;
        index += curLevel * curLevel * ( (3U * regionX) ^ regionY);
        if( regionY == 0U )
        {
            if( regionX == 1U )
            {
                posX = uint32_t( (XE_HILBERT_WIDTH - 1U) ) - posX;
                posY = uint32_t( (XE_HILBERT_WIDTH - 1U) ) - posY;
            }

            uint32_t temp = posX;
            posX = posY;
            posY = temp;
        }
    }
    return index;
}

void initGTAO(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    u_XeGTAOData = bgfx::createUniform("u_XeGTAOData", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_XeGTAOData))
    {
        logger->error("Failed to create uniform: u_XeGTAOData");
        throw std::runtime_error("Failed to create uniform: u_XeGTAOData");
    }

    XeGTAO_PrefilterDepths16x16Program =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_XeGTAO_PrefilterDepths16x16"), true);
    if (!bgfx::isValid(XeGTAO_PrefilterDepths16x16Program))
    {
        logger->error("Failed to create XeGTAO Prefilter Depths 16x16 program.");
        throw std::runtime_error("Failed to create XeGTAO Prefilter Depths 16x16 program.");
    }

    XeGTAO_MainPassProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_XeGTAO_MainPass"), true);
    if (!bgfx::isValid(XeGTAO_MainPassProgram))
    {
        logger->error("Failed to create XeGTAO Main Pass program.");
        throw std::runtime_error("Failed to create XeGTAO Main Pass program.");
    }

    XeGTAO_debugNormalsProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_XeGTAO_debugNormals"), true);
    if (!bgfx::isValid(XeGTAO_debugNormalsProgram))
    {
        logger->error("Failed to create XeGTAO Debug Normals program.");
        throw std::runtime_error("Failed to create XeGTAO Debug Normals program.");
    }

    XeGTAO_debugVisibilityProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_XeGTAO_debugVisibility"), true);
    if (!bgfx::isValid(XeGTAO_debugVisibilityProgram))
    {
        logger->error("Failed to create XeGTAO Debug Visibility program.");
        throw std::runtime_error("Failed to create XeGTAO Debug Visibility program.");
    }

    TEXTURE(
        gGTAOWorkingDepth,
        width, height,
        1.0f, 1.0f,
        true,
        1,
        bgfx::TextureFormat::R32F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);
    gGTAOWorkingDepth.mipCount = XE_GTAO_DEPTH_MIP_LEVELS;

    TEXTURE(
        gGTAOWorkingAOTerm,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R32U,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gGTAOWorkingEdges,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R32F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    // Hilbert look-up texture! It's a 64 x 64 uint16 texture generated using HilbertIndex
    {
        uint16_t * data = new uint16_t[64*64];
        for( int x = 0; x < 64; x++ )
        {
            for( int y = 0; y < 64; y++ )
            {
                uint32_t r2index = HilbertIndex( x, y );
                assert( r2index < 65536 );
                data[ x + 64*y ] = (uint16_t)r2index;
            }
        }

        TEXTURE_MEMORY(
            gGTAOHilbertLut,
            64, 64,
            1.0f, 1.0f,
            false,
            1,
            bgfx::TextureFormat::R16U,
            BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
            bgfx::copy(data, 64*64*sizeof(uint16_t))
            );

        delete[] data;
    }
}

inline constexpr bx::Quaternion rotation3dToQuaternion(const frc::Rotation3d &rotation)
{
    const frc::Quaternion &frcQuat = rotation.GetQuaternion();
    return {static_cast<float>(frcQuat.X()),
            static_cast<float>(frcQuat.Y()),
            static_cast<float>(frcQuat.Z()),
            static_cast<float>(frcQuat.W())};
}

inline constexpr bx::Quaternion rotation3dToQuaternionInverse(const frc::Rotation3d &rotation)
{
    const frc::Quaternion frcQuat = rotation.GetQuaternion().Inverse();
    return {static_cast<float>(frcQuat.X()),
            static_cast<float>(frcQuat.Y()),
            static_cast<float>(frcQuat.Z()),
            static_cast<float>(frcQuat.W())};
}

typedef struct Transform
{
    float matrix[16];

    Transform()
    {
        bx::mtxIdentity(matrix);
    }

    Transform(float *modelMatrix, bx::Vec3 position, bx::Quaternion rotation)
    {
        float rotationMatrix[16];
        bx::mtxFromQuaternion(rotationMatrix, rotation);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
        float relativeMatrix[16];
        bx::mtxMul(relativeMatrix, rotationMatrix, translationMatrix);
        bx::mtxMul(matrix, modelMatrix, relativeMatrix);
    }

    Transform(float *modelMatrix, float *parentMatrix, bx::Vec3 position, bx::Quaternion rotation)
    {
        float rotationMatrix[16];
        bx::mtxFromQuaternion(rotationMatrix, rotation);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
        float relativeMatrix[16];
        bx::mtxMul(relativeMatrix, rotationMatrix, translationMatrix);
        float finalMatrix[16];
        bx::mtxMul(finalMatrix, modelMatrix, relativeMatrix);
        bx::mtxMul(matrix, finalMatrix, parentMatrix);
    }

    Transform(bx::Vec3 position, bx::Quaternion rotation)
    {
        float rotationMatrix[16];
        bx::mtxFromQuaternion(rotationMatrix, rotation);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
        bx::mtxMul(matrix, rotationMatrix, translationMatrix);
    }
} Transform;

typedef struct InstanceData
{
    Transform transform;
    Transform previousTransform;
} InstanceData;

struct UVVertex
{
    float x, y, z;
    float u, v;

    static bgfx::VertexLayout layout;

    static void init()
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }
};

bgfx::VertexLayout UVVertex::layout;

std::vector<Mesh> fieldMeshes;

typedef struct DynamicObjectData
{
    int lastDataUpdate = -1;
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    bx::Quaternion rotation = rotation3dToQuaternion(frc::Rotation3d());

    bx::Vec3 lastPosition = {0.0f, 0.0f, 0.0f};
    bx::Quaternion lastRotation = rotation3dToQuaternion(frc::Rotation3d());

    InstanceData instanceData = {};

    void update(float *modelMatrix, float *parentMatrix, float deltaTime)
    {
        if (!freezeTemporalEffects)
        {
            instanceData.previousTransform = instanceData.transform;
        }

        if (lastDataUpdate == -1)
        {
            lastPosition = position;
            lastRotation = rotation;
        }
        else
        {
            float interpolationFactor = deltaTime / settings::ntPeriodic;
            lastPosition = bx::lerp(lastPosition, position, interpolationFactor);
            lastRotation = bx::lerp(lastRotation, rotation, interpolationFactor);
        }

        if (parentMatrix == nullptr)
        {
            instanceData.transform = {
                modelMatrix,
                lastPosition,
                lastRotation};
        }
        else
        {
            instanceData.transform = {
                modelMatrix,
                parentMatrix,
                lastPosition,
                lastRotation};
        }

        if (lastDataUpdate == -1)
        {
            instanceData.previousTransform = instanceData.transform;
        }
    }

    void update(float *modelMatrix, float deltaTime)
    {
        update(modelMatrix, nullptr, deltaTime);
    }
} DynamicObjectData;

typedef struct RobotComponentData
{
    DynamicObjectData dynamicData;
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;
} RobotComponentData;

typedef struct RobotData
{
    DynamicObjectData dynamicData;
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;

    std::vector<RobotComponentData> components;

    nt::StructTopic<frc::Pose3d> poseTopic;
    nt::StructSubscriber<frc::Pose3d> poseSub;

    nt::StructArrayTopic<frc::Pose3d> componentPosesTopic;
    nt::StructArraySubscriber<frc::Pose3d> componentPosesSub;
} RobotData;

typedef struct GamePieceData
{
    std::string name;
    std::unordered_map<int, DynamicObjectData> instances;
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;

    nt::StructArrayTopic<frc::Pose3d> posesTopic;
    nt::StructArraySubscriber<frc::Pose3d> posesSub;

    nt::StructArrayTopic<Pose3dObject> poseObjectsTopic;
    nt::StructArraySubscriber<Pose3dObject> poseObjectsSub;
} GamePieceData;

std::vector<RobotData> robots;
std::vector<GamePieceData> gamePieces;

static std::function<void()> restartSimulationCallback;

void field::setRestartSimulationCallback(std::function<void()> callback)
{
    restartSimulationCallback = callback;
}

struct OrbitCamera
{
    bx::Vec3 target{0.0f, 0.0f, 0.25f};
    float distance = 15.811388f;
    float yaw = bx::toRad(90.0f);
    float pitch = bx::toRad(18.0f);
    float minDistance = 0.1f;
    float maxDistance = 120.0f;
    float orbitSpeed = 0.008f;

    float originTransform[16];

    OrbitCamera()
    {
        bx::mtxIdentity(originTransform);
    }
};

OrbitCamera orbitCamera;

void updateOrbitCameraFromInput()
{
    if (!ImGui::GetCurrentContext())
    {
        return;
    }

    ImGuiIO &io = ImGui::GetIO();
    if (io.WantCaptureMouse)
    {
        return;
    }

    if (io.MouseWheel != 0.0f)
    {
        orbitCamera.distance *= std::pow(0.9f, io.MouseWheel);
        orbitCamera.distance = std::clamp(orbitCamera.distance, orbitCamera.minDistance, orbitCamera.maxDistance);
    }

    if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
    {
        orbitCamera.yaw += io.MouseDelta.x * orbitCamera.orbitSpeed;
        orbitCamera.pitch += io.MouseDelta.y * orbitCamera.orbitSpeed;

        constexpr float pitchLimit = bx::toRad(89.0f);
        orbitCamera.pitch = std::clamp(orbitCamera.pitch, -pitchLimit, pitchLimit);
    }
    else if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        bx::Vec3 forward{
            std::cos(orbitCamera.pitch) * std::cos(orbitCamera.yaw),
            -std::cos(orbitCamera.pitch) * std::sin(orbitCamera.yaw),
            std::sin(orbitCamera.pitch)};
        bx::Vec3 right = bx::normalize(bx::cross({0.0f, 0.0f, 1.0f}, forward));
        bx::Vec3 up = bx::cross(forward, right);

        orbitCamera.target = bx::add(orbitCamera.target, bx::mul(bx::add(bx::mul(right, -io.MouseDelta.x), bx::mul(up, io.MouseDelta.y)), orbitCamera.distance * 0.001f));
    }
}

bx::Vec3 getOrbitEye()
{
    const float cosPitch = std::cos(orbitCamera.pitch);
    const float sinPitch = std::sin(orbitCamera.pitch);
    const float sinYaw = std::sin(orbitCamera.yaw);
    const float cosYaw = std::cos(orbitCamera.yaw);

    return {
        orbitCamera.target.x + orbitCamera.distance * cosPitch * cosYaw,
        orbitCamera.target.y - orbitCamera.distance * cosPitch * sinYaw,
        orbitCamera.target.z + orbitCamera.distance * sinPitch};
}

static void performRotationStack(float out[16], const std::vector<ModelRotationConfig> &rotations)
{
    float result[16];
    float output[16];
    bx::mtxIdentity(result);

    for (const auto &rotation : rotations)
    {
        float rotationMtx[16];
        switch (rotation.axis)
        {
            // invert x and y axis rotations to match advscope
        case ModelRotationAxis::X:
            bx::mtxRotateX(rotationMtx, -bx::toRad(rotation.angleDegrees));
            break;
        case ModelRotationAxis::Y:
            bx::mtxRotateY(rotationMtx, -bx::toRad(rotation.angleDegrees));
            break;
        case ModelRotationAxis::Z:
            bx::mtxRotateZ(rotationMtx, -bx::toRad(rotation.angleDegrees));
            break;
        }

        bx::mtxMul(output, result, rotationMtx);
        std::memcpy(result, output, sizeof(float) * 16);
    }

    std::memcpy(out, result, sizeof(float) * 16);
}

static frc::Pose3d transformPose3dToLocalCoordinates(const frc::Pose3d &pose)
{
    switch (coordinateSystem)
    {
    case WPILibCoordinateSystem::WallBlue:
        return frc::Pose3d(
            units::meter_t{fieldWidthMeters} / 2.0 - pose.X(),
            units::meter_t{fieldHeightMeters} / 2.0 - pose.Y(),
            pose.Z(),
            frc::Rotation3d(units::degree_t{0.0}, units::degree_t{0.0}, units::degree_t{180.0}).RotateBy(-pose.Rotation()));
    case WPILibCoordinateSystem::CenterRed:
        return pose;
    default:
        throw std::runtime_error("Invalid coordinate system");
    }
}

/**
 * tags is a map of mesh-name:tag
 * meshes of the same material but different tags will stay separated and won't be merged.
 * this is useful for dynamically removing meshes based on tags.
 * additionally, meshes with same tag but different materials will also stay separated, since they can't be merged anyway.
 */
void loadAndCacheMeshes(std::vector<Mesh> &meshes, std::string directory, std::string name, const std::unordered_map<std::string, std::string> &tags)
{
    std::filesystem::path glbPath = directory + name + ".glb";
    std::filesystem::path cachePath = directory + name + ".cache";

    if (settings::cacheModels && std::filesystem::exists(cachePath))
    {
        logger->info("Loading {0} meshes from cache.", directory + name);
        try
        {
            Mesh::fromSerialized(meshes, cachePath);
            return;
        }
        catch (const std::exception &e)
        {
            logger->warn("Failed to load meshes from cache: {}. Updating from GLTF model instead.", e.what());
        }
    }

    logger->info("Loading {0} meshes from GLTF model.", directory + name);
    fastgltf::Parser parser;
    Mesh::fromGltfModel(meshes, parser.loadGltfBinary(fastgltf::GltfDataBuffer::FromPath(glbPath.string()).get(), directory, fastgltf::Options::LoadGLBBuffers | fastgltf::Options::DontRequireValidAssetMember).get(), tags);

    if (settings::cacheModels)
    {
        logger->info("Caching {0} meshes to disk.", directory + name);
        Mesh::toSerialized(meshes, cachePath);
    }
}

void loadFieldModel()
{
    logger->info("Loading field model");
    bx::DefaultAllocator allocator;
    bx::FileReader reader;
    bx::Error err;

    std::string fieldDirectory = std::string(SDL_GetPrefPath(NULL, "DriverSim")) + "field/";
    std::string configFile = fieldDirectory + "config.json";
    if (bx::open(&reader, configFile.c_str(), &err))
    {
        uint32_t size = (uint32_t)bx::getSize(&reader);

        char *data = (char *)bx::alloc(&allocator, size + 1);
        bx::read(&reader, data, size, &err);
        data[size] = '\0';

        nlohmann::json j = nlohmann::json::parse(data);

        bx::close(&reader);
        bx::free(&allocator, data);

        logger->info("Loaded field config file: {0}", configFile);

        performRotationStack(fieldModelMatrix, j["rotations"].get<std::vector<ModelRotationConfig>>());

        coordinateSystem = coordinateSystemFromString(j["coordinateSystem"].get<std::string>());
        fieldWidthMeters = j["widthInches"].get<float>() * INCHES_TO_METERS;
        fieldHeightMeters = j["heightInches"].get<float>() * INCHES_TO_METERS;

        std::unordered_map<std::string, std::string> tags;

        for (const auto &gamePiece : j["gamePieces"])
        {
            std::string name = gamePiece["name"];
            auto position = gamePiece["position"];
            float rotationMatrix[16];
            performRotationStack(rotationMatrix, gamePiece["rotations"].get<std::vector<ModelRotationConfig>>());
            float translationMatrix[16];
            bx::mtxTranslate(translationMatrix, position[0].get<float>(), position[1].get<float>(), position[2].get<float>());
            float modelMatrix[16];
            bx::mtxMul(modelMatrix, rotationMatrix, translationMatrix);

            gamePieces.push_back({
                .name = name,
                .modelMatrix = std::to_array(modelMatrix),
            });

            for (const auto &stagedObject : gamePiece["stagedObjects"].get<std::vector<std::string>>())
            {
                tags[stagedObject] = name;
            }
        }

#if GAME_YEAR == 2026
        Rebuilt2026::addHubLedTags(tags);
#endif

        loadAndCacheMeshes(fieldMeshes, fieldDirectory, "model", tags);

        for (size_t i = 0; i < gamePieces.size(); i++)
        {
            loadAndCacheMeshes(gamePieces[i].meshes, fieldDirectory, "model_" + std::to_string(i), {});
            for (auto &mesh : gamePieces[i].meshes)
            {
                mesh.material.writesObjectMotionVectors = true;
            }
        }

        logger->info("Field initialized successfully.");
    }
    else
    {
        logger->error("Could not open field config file: {0}, error: {1}", configFile, err.getMessage().getCPtr());
    }
}

void loadRobotModel()
{
    logger->info("Loading robot model");
    bx::DefaultAllocator allocator;
    bx::FileReader reader;
    bx::Error err;

    std::string robotDirectory = std::string(SDL_GetPrefPath(NULL, "DriverSim")) + "robot/";
    std::string configFile = robotDirectory + "config.json";
    if (bx::open(&reader, configFile.c_str(), &err))
    {
        uint32_t size = (uint32_t)bx::getSize(&reader);

        char *data = (char *)bx::alloc(&allocator, size + 1);
        bx::read(&reader, data, size, &err);
        data[size] = '\0';

        nlohmann::json j = nlohmann::json::parse(data);

        bx::close(&reader);
        bx::free(&allocator, data);

        logger->info("Loaded robot config file: {0}", configFile);

        float rotationMtx[16];
        performRotationStack(rotationMtx, j["rotations"].get<std::vector<ModelRotationConfig>>());
        const auto &position = j["position"].get<std::vector<float>>();
        float translationMtx[16];
        bx::mtxTranslate(translationMtx, position[0], position[1], position[2]);
        float robotModelMatrix[16];
        bx::mtxMul(robotModelMatrix, rotationMtx, translationMtx);

        std::vector<RobotComponentData> components;
        for (const auto &component : j["components"])
        {
            float componentRotationMtx[16];
            performRotationStack(componentRotationMtx, component["zeroedRotations"].get<std::vector<ModelRotationConfig>>());
            const auto &componentPosition = component["zeroedPosition"].get<std::vector<float>>();
            float componentTranslationMtx[16];
            bx::mtxTranslate(componentTranslationMtx, componentPosition[0], componentPosition[1], componentPosition[2]);
            float componentModelMatrix[16];
            bx::mtxMul(componentModelMatrix, componentRotationMtx, componentTranslationMtx);

            components.push_back({
                .modelMatrix = std::to_array(componentModelMatrix),
            });
        }

        robots.push_back({.modelMatrix = std::to_array(robotModelMatrix),
                          .components = components});

        loadAndCacheMeshes(robots.back().meshes, robotDirectory, "model", {});
        for (auto &mesh : robots.back().meshes)
        {
            mesh.material.writesObjectMotionVectors = true;
        }
        for (size_t i = 0; i < components.size(); i++)
        {
            loadAndCacheMeshes(robots.back().components[i].meshes, robotDirectory, "model_" + std::to_string(i), {});
            for (auto &mesh : robots.back().components[i].meshes)
            {
                mesh.material.writesObjectMotionVectors = true;
            }
        }

        logger->info("Robot initialized successfully.");
    }
    else
    {
        logger->error("Could not open robot config file: {0}, error: {1}", configFile, err.getMessage().getCPtr());
    }
}

bool startedLoadingFieldModel = false;

void field::startLoadFieldModel()
{
    if (startedLoadingFieldModel)
    {
        return;
    }

    startedLoadingFieldModel = true;

    logger->info("Starting loading field model in background thread.");
    fieldModelLoadingFuture = std::async(std::launch::async, loadFieldModel);
}

bool startedLoadingRobotModel = false;

void field::startLoadRobotModel()
{
    if (startedLoadingRobotModel)
    {
        return;
    }

    startedLoadingRobotModel = true;

    logger->info("Starting loading robot model in background thread.");
    robotModelLoadingFuture = std::async(std::launch::async, loadRobotModel);
}

void field::init(const blackboard::app::Window &window)
{
    MeshVertex::init();
    UVVertex::init();

    s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    if (!bgfx::isValid(s_tex))
    {
        logger->error("Failed to create uniform for generic texture.");
        throw std::runtime_error("Failed to create uniform for generic texture.");
    }

    const auto type = bgfx::getRendererType();

    blitProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_blit"), true);

    if (!bgfx::isValid(blitProgram))
    {
        logger->error("Failed to create blit program.");
        throw std::runtime_error("Failed to create blit program.");
    }

    debugNormalsProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_debug_normals"), true);

    if (!bgfx::isValid(debugNormalsProgram))
    {
        logger->error("Failed to create debug normals program.");
        throw std::runtime_error("Failed to create debug normals program.");
    }

    initTAA(window.width, window.height); // init first to define uniforms
    initGBuffer(window.width, window.height);
    initPBROIT(window.width, window.height);
    initTonemap();
    initMotionBlur(window.width, window.height);
    initBloom(window.width, window.height);
    initGTAO(window.width, window.height);
}

void field::startNTClient()
{
    logger->info("Starting NetworkTables client");
    ntInst = nt::NetworkTableInstance::Create();
    ntInst.AddConnectionListener(true, [](const nt::Event &event)
                                 {
                                    if (event.Is(nt::EventFlags::kConnected))
                                    {
                                        auto connInfo = std::get<nt::ConnectionInfo>(event.data);
                                        logger->info("Connected to NetworkTables server at {}:{}", connInfo.remote_ip, connInfo.remote_port);
                                    }
                                    else if( event.Is(nt::EventFlags::kDisconnected))
                                    {
                                        auto connInfo = std::get<nt::ConnectionInfo>(event.data);
                                        logger->warn("Disconnected from NetworkTables server at {}:{}", connInfo.remote_ip, connInfo.remote_port);
                                    } });

    fmsUI = std::make_unique<Rebuilt2026FMSUI>(ntInst);

    ntInst.SetServer("127.0.0.1");
    ntInst.StartClient4("driver-sim");
}

// assumes reverse z, infinite far
void updateInfo(bgfx::Encoder *encoder, float cameraNear, float proj[16])
{
    float info[8] = {
        cameraNear,
        2.0f / proj[0],
        -2.0f / proj[5],
        -1.0f / proj[0],
        1.0f / proj[5],
        0.0f,
        0.0f,
        0.0f
    };
    encoder->setUniform(u_info, info, 2);
}

void screenSpaceQuad(bool _originBottomLeft, bgfx::Encoder *encoder, float _width = 1.0f, float _height = 1.0f)
{
    if (3 == bgfx::getAvailTransientVertexBuffer(3, UVVertex::layout))
    {
        bgfx::TransientVertexBuffer vb;
        bgfx::allocTransientVertexBuffer(&vb, 3, UVVertex::layout);
        UVVertex *vertex = (UVVertex *)vb.data;

        const float minx = -_width;
        const float maxx = _width;
        const float miny = 0.0f;
        const float maxy = _height * 2.0f;

        const float minu = -1.0f;
        const float maxu = 1.0f;

        const float zz = 0.0f;

        float minv = 0.0f;
        float maxv = 2.0f;

        if (_originBottomLeft)
        {
            float temp = minv;
            minv = maxv;
            maxv = temp;

            minv -= 1.0f;
            maxv -= 1.0f;
        }

        vertex[0].x = minx;
        vertex[0].y = miny;
        vertex[0].z = zz;
        vertex[0].u = minu;
        vertex[0].v = minv;

        vertex[1].x = maxx;
        vertex[1].y = miny;
        vertex[1].z = zz;
        vertex[1].u = maxu;
        vertex[1].v = minv;

        vertex[2].x = maxx;
        vertex[2].y = maxy;
        vertex[2].z = zz;
        vertex[2].u = maxu;
        vertex[2].v = maxv;

        encoder->setVertexBuffer(0, &vb);
    }
}

float previousViewProj[16] = {0};
float previousView[16] = {0};
float previousProj[16] = {0};
float curTime = 0.0f;
float previousJitterX = 0.0f;
float previousJitterY = 0.0f;
bool firstFrame = true;

bool firstTAAFrame = true;
bool taaUseBuffer1 = false;
uint8_t jitterIndex = 0;

uint8_t mbIndex = 0;

uint8_t gtaoNoiseIndex = 0;

float Halton(uint32_t i, uint32_t b)
{
    float f = 1.0f;
    float r = 0.0f;

    while (i > 0)
    {
        f /= static_cast<float>(b);
        r = r + f * static_cast<float>(i % b);
        i = static_cast<uint32_t>(floorf(static_cast<float>(i) / static_cast<float>(b)));
    }

    return r;
}

static constexpr uint32_t HALTON_SAMPLES = 8;

void ensureTextures(uint16_t width, uint16_t height)
{
    gAccumTex.beginFrame();
    gRevealTex.beginFrame();

    gbufAlbedo.beginFrame();
    gbufEmission.beginFrame();
    gbufNormal.beginFrame();
    gbufPBRData.beginFrame();
    gbufVelocity.beginFrame();
    gFullVelocity.beginFrame();
    gbufDepth.beginFrame();

    gOutputColor.beginFrame();

    gTAABuffer0.beginFrame();
    gTAABuffer1.beginFrame();

    gMBTileMaxX.beginFrame();
    gMBTileMax.beginFrame();
    gMBNeighborMax.beginFrame();
    gMBTileVariance.beginFrame();
    gMBOutputColor.beginFrame();
    gMBVelocity.beginFrame();

    gGTAOWorkingDepth.beginFrame();
    gGTAOWorkingAOTerm.beginFrame();
    gGTAOWorkingEdges.beginFrame();

    gAccumTex.ensure(width, height);
    gRevealTex.ensure(width, height);

    gOitFbo.ensure(width, height);
    gOitDepthPostPassFbo.ensure(width, height);

    gbufAlbedo.ensure(width, height);
    gbufEmission.ensure(width, height);
    gbufNormal.ensure(width, height);
    gbufPBRData.ensure(width, height);
    gbufVelocity.ensure(width, height);
    gFullVelocity.ensure(width, height);
    gbufDepth.ensure(width, height);

    gBufFbo.ensure(width, height);

    gOutputColor.ensure(width, height);
    gOutputColor.mipCount = calculateBloomMipmapLevels(gOutputColor.width, gOutputColor.height);

    gTAABuffer0.ensure(width, height);
    gTAABuffer1.ensure(width, height);

    gMBTileMaxX.widthFraction = 1.0f / MB_TILE_SIZE;
    gMBTileMax.widthFraction = 1.0f / MB_TILE_SIZE;
    gMBTileMax.heightFraction = 1.0f / MB_TILE_SIZE;
    gMBNeighborMax.widthFraction = 1.0f / MB_TILE_SIZE;
    gMBNeighborMax.heightFraction = 1.0f / MB_TILE_SIZE;
    gMBTileVariance.widthFraction = 1.0f / MB_TILE_SIZE;
    gMBTileVariance.heightFraction = 1.0f / MB_TILE_SIZE;

    gMBTileMaxX.ensure(width, height);
    gMBTileMax.ensure(width, height);
    gMBNeighborMax.ensure(width, height);
    gMBTileVariance.ensure(width, height);
    gMBOutputColor.ensure(width, height);
    gMBVelocity.ensure(width, height);

    gGTAOWorkingDepth.ensure(width, height);
    gGTAOWorkingDepth.mipCount = XE_GTAO_DEPTH_MIP_LEVELS;
    gGTAOWorkingAOTerm.ensure(width, height);
    gGTAOWorkingEdges.ensure(width, height);
}

void setupMesh(bgfx::Encoder *encoder, const Mesh &mesh, bool forceDepthTest)
{
    float pbrData[4] = {
        (mesh.material.writesObjectMotionVectors && settings::writeObjectMotionVectors) ? 1.0f : 0.0f,
        mesh.material.metallic,
        mesh.material.roughness,
        0.0f};

    if (!forceDepthTest && mesh.material.type == MaterialType::Transparent)
    {
        encoder->setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_INDEPENDENT |
                BGFX_STATE_DEPTH_TEST_GREATER
                // RT0
                | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                        BGFX_STATE_BLEND_ONE),
            // RT1
            BGFX_STATE_BLEND_FUNC_RT_1(BGFX_STATE_BLEND_ZERO,
                                       BGFX_STATE_BLEND_INV_SRC_ALPHA));
    }
    else
    {
        encoder->setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_GREATER |
            BGFX_STATE_WRITE_Z);
    }

    encoder->setVertexBuffer(0, mesh.vertexBuffer);
    encoder->setIndexBuffer(mesh.indexBuffer);

    encoder->setUniform(u_baseColor, SRGBToLinear(mesh.material.baseColor).data());
    encoder->setUniform(u_emissionColor, SRGBToLinear(mesh.material.emissionColor).data());
    encoder->setUniform(u_previousModelViewProj, previousViewProj);
    encoder->setUniform(u_pbrData, pbrData);

    if (mesh.material.texture == "carpet")
    {
        encoder->setTexture(0, s_baseColor, carpetBaseColor.handle);
        encoder->setTexture(1, s_bump, carpetBump.handle);
    }
}

template <std::ranges::input_range R>
void drawMeshes(bgfx::Encoder *encoder, R &&meshes, float modelMatrix[16])
{
    for (const auto &mesh : meshes)
    {
        setupMesh(encoder, mesh, false);
        encoder->setTransform(modelMatrix);
        if (mesh.material.type == MaterialType::Transparent)
        {
            encoder->submit(VIEW_OIT, programOit);

#if 0 // IF TRANSPARENT MOTION VECTORS AND DEPTH (transparent TAA)
            setupMesh(encoder, mesh, true);
            encoder->setTransform(modelMatrix);
            encoder->submit(VIEW_OIT_DEPTH_POST_PASS, programOitDepthPostPass);
#endif
        }
        else
        {
            encoder->submit(VIEW_GBUFFER, mesh.material.texture.empty() ? programPBR : programPBRTextured);
        }
    }
}

void drawMeshesInstanced(bgfx::Encoder *encoder, const std::vector<Mesh> &meshes, const std::vector<InstanceData> &instances)
{
    // figure out how big of a buffer is available
    uint32_t instanceCount = bgfx::getAvailInstanceDataBuffer(instances.size(), sizeof(InstanceData));

    bgfx::InstanceDataBuffer idb;
    bgfx::allocInstanceDataBuffer(&idb, instanceCount, sizeof(InstanceData));

    std::memcpy(idb.data, instances.data(), instanceCount * sizeof(InstanceData));

    for (const auto &mesh : meshes)
    {
        setupMesh(encoder, mesh, false);
        encoder->setInstanceDataBuffer(&idb);
        if (mesh.material.type == MaterialType::Transparent)
        {
            encoder->submit(VIEW_OIT, programOitInstanced);

#if 0 // IF TRANSPARENT MOTION VECTORS AND DEPTH (transparent TAA)
            setupMesh(encoder, mesh, true);
            encoder->setInstanceDataBuffer(&idb);
            encoder->submit(VIEW_OIT_DEPTH_POST_PASS, programOitDepthPostPassInstanced);
#endif
        }
        else
        {
            encoder->submit(VIEW_GBUFFER, programPBRInstanced);
        }
    }
}

bool createdFieldMeshBuffers = false;
bool createdRobotMeshBuffers = false;
int currentDataUpdateIndex = 0;

std::array<float, 4> skyColor = SRGBToLinear({0.54f, 0.54f, 0.6f, 1.0f});
std::array<std::array<float, 4>, LIGHT_COUNT> lightColor = {
    SRGBToLinear({1.0f, 0.25f, 0.25f, 432.0f}),
    SRGBToLinear({1.0f, 0.85f, 0.85f, 332.0f}),
    SRGBToLinear({1.0f, 1.0f, 1.0f, 432.0f}),
    SRGBToLinear({1.0f, 1.0f, 1.0f, 332.0f}),
    SRGBToLinear({0.25f, 0.45f, 1.0f, 432.0f}),
    SRGBToLinear({0.65f, 0.85f, 1.0f, 332.0f})};

void field::render(const blackboard::app::Window &window)
{
    currentDataUpdateIndex = (currentDataUpdateIndex + 1) % 1000000;
    ImGui::Begin("Options");
    ImGui::Checkbox("Freeze Temporal Effects", &freezeTemporalEffects);
    ImGui::Checkbox("Write Object Motion Vectors", &settings::writeObjectMotionVectors);
    ImGui::Checkbox("Enable Motion Blur", &settings::enableMotionBlur);
    ImGui::Checkbox("Enable TAA", &settings::enableTAA);
    ImGui::Checkbox("Enable Bloom", &settings::enableBloom);

    ImGui::Separator();

    ImGui::Text("Motion Blur Settings");
    int tileSize = MB_TILE_SIZE;
    ImGui::SliderInt("Tile Size", &tileSize, 8, 128);
    MB_TILE_SIZE = static_cast<uint8_t>(tileSize);
    int sampleCount = MB_SAMPLE_COUNT;
    ImGui::SliderInt("Sample Count", &sampleCount, 4, 64);
    MB_SAMPLE_COUNT = static_cast<uint8_t>(sampleCount);

    ImGui::Separator();
    ImGui::SliderFloat("Exposure", &tonemappingExposure, -15.0f, 10.0f);

    ImGui::Separator();

    ImGui::Text("GTAO Settings");

    ImGui::SliderFloat("Effect Radius", &gtaoEffectRadius, 0.01f, 3.0f);
    ImGui::SliderFloat("Radius Multiplier", &gtaoRadiusMultiplier, 0.25f, 2.0f);
    ImGui::SliderFloat("Effect Falloff Range", &gtaoEffectFalloffRange, 0.0f, 1.0f);

    ImGui::Separator();

    if (ImGui::BeginCombo("Debug View", DEBUG_VIEW_NAMES[static_cast<int>(debugView)].c_str()))
    {
        for (int i = 0; i < DEBUG_VIEW_NAMES.size(); i++)
        {
            if (ImGui::Selectable(DEBUG_VIEW_NAMES[i].c_str(), debugView == static_cast<DebugView>(i)))
            {
                debugView = static_cast<DebugView>(i);
            }
        }

        ImGui::EndCombo();
    }

    ImGui::Separator();

    if (ImGui::BeginCombo("Camera View", CAMERA_VIEW_NAMES[static_cast<int>(cameraView)].c_str()))
    {
        for (int i = 0; i < CAMERA_VIEW_NAMES.size(); i++)
        {
            if (ImGui::Selectable(CAMERA_VIEW_NAMES[i].c_str(), cameraView == static_cast<CameraView>(i)))
            {
                cameraView = static_cast<CameraView>(i);
                orbitCamera.target = {0.0f, 0.0f, 0.25f};
                bx::mtxIdentity(orbitCamera.originTransform);
            }
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Restart Simulation"))
    {
        if (restartSimulationCallback)
        {
            restartSimulationCallback();
        }
    }

    ImGui::End();

    if (fmsUI)
    {
        fmsUI->render(ImGui::GetMainViewport()->Size);
    }

    if (!createdFieldMeshBuffers && fieldModelLoadingFuture.valid() &&
        fieldModelLoadingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        for (auto &gamePiece : gamePieces)
        {
            gamePiece.posesTopic = ntInst.GetStructArrayTopic<frc::Pose3d>("/AdvantageKit/RealOutputs/RobotModel/" + gamePiece.name + "Positions");
            gamePiece.posesSub = gamePiece.posesTopic.Subscribe({}, {.periodic = settings::ntPeriodic});
            gamePiece.poseObjectsTopic = ntInst.GetStructArrayTopic<Pose3dObject>("/AdvantageKit/RealOutputs/RobotModel/" + gamePiece.name + "Objects");
            gamePiece.poseObjectsSub = gamePiece.poseObjectsTopic.Subscribe({}, {.periodic = settings::ntPeriodic});
            Mesh::createBuffersForMeshes(gamePiece.meshes);
        }

        Mesh::createBuffersForMeshes(fieldMeshes);

        fmsUI->postProcessField(fieldMeshes);

        createdFieldMeshBuffers = true;
    }

    if (!createdRobotMeshBuffers && robotModelLoadingFuture.valid() &&
        robotModelLoadingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        for (auto &robot : robots)
        {
            robot.poseTopic = ntInst.GetStructTopic<frc::Pose3d>("/AdvantageKit/RealOutputs/RobotModel/Robot");
            robot.poseSub = robot.poseTopic.Subscribe(frc::Pose3d{}, {.periodic = settings::ntPeriodic});
            robot.componentPosesTopic = ntInst.GetStructArrayTopic<frc::Pose3d>("/AdvantageKit/RealOutputs/RobotModel/MechanismPoses");
            robot.componentPosesSub = robot.componentPosesTopic.Subscribe({}, {.periodic = settings::ntPeriodic});
        }

        Mesh::createBuffersForMeshes(robots.back().meshes);
        for (size_t i = 0; i < robots.back().components.size(); i++)
        {
            Mesh::createBuffersForMeshes(robots.back().components[i].meshes);
        }
        createdRobotMeshBuffers = true;
    }

    const float deltaTime = ImGui::GetIO().DeltaTime;
    curTime += deltaTime;

    // Dynamic objects
    for (auto &robot : robots)
    {
        auto robotPose = robot.poseSub.GetAtomic();
        if (robot.poseSub.Exists())
        {
            frc::Pose3d localPose = transformPose3dToLocalCoordinates(robotPose.value);
            robot.dynamicData.position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
            robot.dynamicData.rotation = rotation3dToQuaternion(localPose.Rotation());
            robot.dynamicData.update(robot.modelMatrix.data(), deltaTime);
            robot.dynamicData.lastDataUpdate = currentDataUpdateIndex;

            // Update component poses
            auto componentPoses = robot.componentPosesSub.GetAtomic();
            Transform robotOrigin{robot.dynamicData.lastPosition, robot.dynamicData.lastRotation};
            for (size_t i = 0; i < robot.components.size(); ++i)
            {
                if (robot.componentPosesSub.Exists() && i < componentPoses.value.size())
                {
                    // invert rotation to match advscope
                    robot.components[i].dynamicData.position = {static_cast<float>(componentPoses.value[i].X().value()), static_cast<float>(componentPoses.value[i].Y().value()), static_cast<float>(componentPoses.value[i].Z().value())};
                    robot.components[i].dynamicData.rotation = rotation3dToQuaternionInverse(componentPoses.value[i].Rotation());
                    robot.components[i].dynamicData.update(robot.components[i].modelMatrix.data(), robotOrigin.matrix, deltaTime);
                }
                else
                {
                    robot.components[i].dynamicData.position = {0.0f, 0.0f, 0.0f};
                    robot.components[i].dynamicData.rotation = rotation3dToQuaternion(frc::Rotation3d{});
                    robot.components[i].dynamicData.update(robot.modelMatrix.data(), robotOrigin.matrix, deltaTime);
                }

                robot.components[i].dynamicData.lastDataUpdate = currentDataUpdateIndex;
            }
        }
        else
        {
            robot.dynamicData.lastDataUpdate = -1;
            for (auto &component : robot.components)
            {
                component.dynamicData.lastDataUpdate = -1;
            }
        }
    }

    for (auto &gamePiece : gamePieces)
    {
        bool hasObjects = gamePiece.poseObjectsSub.Exists();
        if (hasObjects || gamePiece.posesSub.Exists())
        {
            auto updateInstance = [&](DynamicObjectData &instance, const frc::Pose3d &pose)
            {
                auto localPose = transformPose3dToLocalCoordinates(pose);
                instance.position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
                instance.rotation = rotation3dToQuaternion(localPose.Rotation());
                instance.update(gamePiece.modelMatrix.data(), deltaTime);
                instance.lastDataUpdate = currentDataUpdateIndex;
            };

            if (hasObjects)
            {
                auto gamePiecePoseObjects = gamePiece.poseObjectsSub.GetAtomic();
                for (size_t i = 0; i < gamePiecePoseObjects.value.size(); ++i)
                {
                    updateInstance(gamePiece.instances[gamePiecePoseObjects.value[i].identity], gamePiecePoseObjects.value[i].pose);
                }
            }
            else
            {
                auto gamePiecePoses = gamePiece.posesSub.GetAtomic();
                for (size_t i = 0; i < gamePiecePoses.value.size(); ++i)
                {
                    updateInstance(gamePiece.instances[i], gamePiecePoses.value[i]);
                }
            }
            std::erase_if(gamePiece.instances, [](const std::pair<int, DynamicObjectData> &pair)
                          { return pair.second.lastDataUpdate != currentDataUpdateIndex; });
        }
        else
        {
            gamePiece.instances.clear();
        }
    }

    if (cameraView == CameraView::Robot || cameraView == CameraView::RobotRelative)
    {
        if (robots.size() > 0 && robots.back().dynamicData.lastDataUpdate == currentDataUpdateIndex)
        {
            float translationMtx[16];
            bx::mtxTranslate(translationMtx, robots.back().dynamicData.lastPosition.x, robots.back().dynamicData.lastPosition.y, robots.back().dynamicData.lastPosition.z);
            if (cameraView == CameraView::RobotRelative)
            {
                // also apply rotation
                float rotationMtx[16];
                bx::mtxFromQuaternion(rotationMtx, robots.back().dynamicData.lastRotation);
                float transformMtx[16];
                bx::mtxMul(transformMtx, rotationMtx, translationMtx);
                bx::mtxInverse(orbitCamera.originTransform, transformMtx);
            }
            else
            {
                bx::mtxInverse(orbitCamera.originTransform, translationMtx);
            }
        }
    }

    updateOrbitCameraFromInput();
    const bx::Vec3 at = orbitCamera.target;
    const bx::Vec3 eye = getOrbitEye();

    uint16_t m_width = window.width;
    uint16_t m_height = window.height;

    ensureTextures(m_width, m_height);

    float jitterX;
    float jitterY;

    if (settings::enableTAA)
    {
        float haltonX = 2.0f * Halton(jitterIndex + 1, 2) - 1.0f;
        float haltonY = 2.0f * Halton(jitterIndex + 1, 3) - 1.0f;
        jitterX = (haltonX / m_width);
        jitterY = (haltonY / m_height);
    }
    else
    {
        jitterX = 0.0f;
        jitterY = 0.0f;
    }

    float lookAt[16];
    bx::mtxLookAt(lookAt, eye, at, {0.0f, 0.0f, 1.0f}, bx::Handedness::Right);
    float view[16];
    bx::mtxMul(view, orbitCamera.originTransform, lookAt);

    float proj[16];
    bx::mtxProjInf(proj, 60.0f, float(m_width) / float(m_height), 0.1f, bgfx::getCaps()->homogeneousDepth, bx::Handedness::Right, bx::NearFar::Reverse);
    proj[8] -= jitterX;
    proj[9] -= jitterY;

    float viewProj[16];
    bx::mtxMul(viewProj, view, proj);

    if (firstFrame)
    {
        std::memcpy(previousViewProj, viewProj, sizeof(viewProj));
        std::memcpy(previousView, view, sizeof(view));
        std::memcpy(previousProj, proj, sizeof(proj));
        firstFrame = false;
    }

    bgfx::Encoder *encoder = bgfx::begin();

    updateInfo(encoder, 0.1f, proj);

    if (!freezeTemporalEffects)
    {
        jitterIndex = (jitterIndex + 1) % HALTON_SAMPLES;
    }

    float jitter[4] = {jitterX, jitterY, previousJitterX, previousJitterY};
    encoder->setUniform(u_jitter, jitter);

    // Light uniforms
    Vec3Padded lightPos[LIGHT_COUNT] = {
        Vec3Padded(bx::mul({-fieldWidthMeters / 2.6f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({-fieldWidthMeters / 2.6f, fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({0.0f, fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({0.0f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({fieldWidthMeters / 2.6f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({fieldWidthMeters / 2.6f, fieldHeightMeters / 2.5f, 6.0f}, view))};

    encoder->setUniform(u_lightPos, lightPos, LIGHT_COUNT);
    encoder->setUniform(u_lightColor, lightColor.data(), LIGHT_COUNT);

    // OPAQUE PASS
    bgfx::setViewName(VIEW_GBUFFER, "Field - GBuffer");
    bgfx::setViewTransform(VIEW_GBUFFER, view, proj);
    bgfx::setViewRect(VIEW_GBUFFER, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_GBUFFER, gBufFbo.handle);
    bgfx::setViewClear(VIEW_GBUFFER,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000,
                       bgfx::getCaps()->homogeneousDepth ? -1.0f : 0.0f);

    // GTAO PASS
    bgfx::setViewName(VIEW_GTAO, "Field - GTAO");
    bgfx::setViewMode(VIEW_GTAO, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(VIEW_GTAO, view, proj);
    bgfx::setViewRect(VIEW_GTAO, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_GTAO, BGFX_INVALID_HANDLE);

    // TRANSPARENT PASS
    bgfx::setViewName(VIEW_OIT, "Field - OIT");
    bgfx::setViewTransform(VIEW_OIT, view, proj);
    bgfx::setViewRect(VIEW_OIT, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_OIT, gOitFbo.handle);
    bgfx::setPaletteColor(0, 0, 0, 0, 0); // Clear accum to 0
    bgfx::setPaletteColor(1, 1, 1, 1, 1); // Clear reveal to 1
    bgfx::setViewClear(VIEW_OIT,
                       BGFX_CLEAR_COLOR,
                       0.0f,
                       0,
                       0,
                       1);

    // Transparent depth post-pass
    bgfx::setViewName(VIEW_OIT_DEPTH_POST_PASS, "Field - OIT Depth Post-Pass");
    bgfx::setViewTransform(VIEW_OIT_DEPTH_POST_PASS, view, proj);
    bgfx::setViewRect(VIEW_OIT_DEPTH_POST_PASS, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_OIT_DEPTH_POST_PASS, gOitDepthPostPassFbo.handle);

    // Post processing
    bgfx::setViewName(VIEW_POSTPROCESS, "Field - Post Process");
    bgfx::setViewMode(VIEW_POSTPROCESS, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(VIEW_POSTPROCESS, view, proj);
    bgfx::setViewRect(VIEW_POSTPROCESS, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_POSTPROCESS, BGFX_INVALID_HANDLE);

    // Blit and tonemap
    bgfx::setViewFrameBuffer(VIEW_BLIT, BGFX_INVALID_HANDLE);
    bgfx::setViewName(VIEW_BLIT, "Field - Blit");
    bgfx::setViewRect(VIEW_BLIT, 0, 0, uint16_t(m_width), uint16_t(m_height));

    if (createdFieldMeshBuffers)
    {
        std::vector<std::string> drawnGamePieces;
        for (auto &gamePiece : gamePieces)
        {
            std::vector<InstanceData> instanceData;
            auto filtered = gamePiece.instances | std::views::filter([](const std::pair<int, DynamicObjectData> &pair)
                                                                     { return pair.second.lastDataUpdate == currentDataUpdateIndex; });
            instanceData.reserve(std::ranges::distance(filtered));
            std::ranges::transform(filtered, std::back_inserter(instanceData),
                                   [](const std::pair<int, DynamicObjectData> &pair)
                                   { return pair.second.instanceData; });
            if (!instanceData.empty())
            {
                drawMeshesInstanced(encoder, gamePiece.meshes, instanceData);
                drawnGamePieces.push_back(gamePiece.name);
            }
        }

        drawMeshes(encoder, fieldMeshes | std::views::filter([&drawnGamePieces](const Mesh &mesh)
                                                             { return std::find(drawnGamePieces.begin(), drawnGamePieces.end(), mesh.tag) == drawnGamePieces.end(); /* only draw meshes that haven't been drawn by game pieces */ }),
                   fieldModelMatrix);
    }

    if (createdRobotMeshBuffers)
    {
        std::vector<InstanceData> instanceData;
        auto filtered = robots | std::views::filter([](const RobotData &robot)
                                                    { return robot.dynamicData.lastDataUpdate == currentDataUpdateIndex; });
        instanceData.reserve(std::ranges::distance(filtered));
        std::ranges::transform(filtered, std::back_inserter(instanceData),
                               [](const RobotData &robot)
                               { return robot.dynamicData.instanceData; });
        if (!instanceData.empty())
        {
            drawMeshesInstanced(encoder, robots.back().meshes, instanceData);
        }

        for (auto &component : robots.back().components)
        {
            if (component.dynamicData.lastDataUpdate != currentDataUpdateIndex)
            {
                continue;
            }

            std::vector<InstanceData> componentInstanceData = {
                component.dynamicData.instanceData};
            drawMeshesInstanced(encoder, component.meshes, componentInstanceData);
        }
    }

    // always clear OIT buffers
    encoder->touch(VIEW_OIT);

    int xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    int yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

    // GTAO

    // force unbind gbuffer view fbo
    encoder->touch(VIEW_GTAO);

    float XeGTAOData[12] = {
        gtaoEffectRadius,
        gtaoRadiusMultiplier,
        gtaoEffectFalloffRange,
        float(gtaoNoiseIndex) + 0.5f,
    
        gtaoSampleDistributionPower,
        gtaoThinOccluderCompensation,
        2.0f / (proj[0] * float(m_width)),
        -2.0f / (proj[5] * float(m_height)),

        gtaoDepthMipSamplingOffset,
        gtaoFinalValuePower,
        0.0f,
        0.0f
    };

    if (!freezeTemporalEffects)
    {
        gtaoNoiseIndex = (gtaoNoiseIndex + 1) % 64;
    }

    encoder->setUniform(u_XeGTAOData, XeGTAOData, 3);

    encoder->setTexture(0, s_depth, gbufDepth.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
    for (int i = 0; i < XE_GTAO_DEPTH_MIP_LEVELS; ++i)
    {
        encoder->setImage(i + 1, gGTAOWorkingDepth.handle, i, bgfx::Access::Write);
    }
    encoder->dispatch(VIEW_GTAO, XeGTAO_PrefilterDepths16x16Program, xGroups, yGroups);

    encoder->setTexture(0, s_depth, gGTAOWorkingDepth.handle, 0, 1, 0, XE_GTAO_DEPTH_MIP_LEVELS, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
    encoder->setImage(1, gbufNormal.handle, 0, bgfx::Access::Read);
    encoder->setImage(2, gGTAOHilbertLut.handle, 0, bgfx::Access::Read);
    encoder->setImage(3, gGTAOWorkingAOTerm.handle, 0, bgfx::Access::Write);
    encoder->setImage(4, gGTAOWorkingEdges.handle, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_GTAO, XeGTAO_MainPassProgram, xGroups, yGroups);

    // Post processing

    encoder->setUniform(u_previousModelViewProj, previousViewProj);

    xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

    // OIT Composition
    encoder->setUniform(u_skyColor, skyColor.data());
    encoder->setImage(0, gAccumTex.handle, 0, bgfx::Access::Read);
    encoder->setImage(1, gRevealTex.handle, 0, bgfx::Access::Read);
    encoder->setImage(2, gbufAlbedo.handle, 0, bgfx::Access::Read);
    encoder->setImage(3, gbufEmission.handle, 0, bgfx::Access::Read);
    encoder->setImage(4, gbufNormal.handle, 0, bgfx::Access::Read);
    encoder->setImage(5, gbufPBRData.handle, 0, bgfx::Access::Read);
    encoder->setTexture(6, s_depth, gbufDepth.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
    encoder->setImage(7, gOutputColor.handle, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_POSTPROCESS, oitCompProgram, xGroups, yGroups);

    // Motion blur Velocity
    float intensity = 1.0f;
    float temp_intensity = intensity;

    if (MB_FRAMERATE_INDEPENDENT)
    {
        float cappedFrameTime = 1.0f / MB_TARGET_CONSTANT_FRAMERATE;

        if (!MB_UNCAPPED_INDEPENDENCE)
        {
            cappedFrameTime = std::min(cappedFrameTime, deltaTime);
        }

        temp_intensity = intensity * cappedFrameTime / deltaTime;
    }

    // Velocity generation
    float mbVelocityData[12] = {
        MB_CAMERA_ROTATION_COMPONENT.multiplier,
        MB_CAMERA_MOVEMENT_COMPONENT.multiplier,
        MB_OBJECT_MOVEMENT_COMPONENT.multiplier,
        MB_CAMERA_ROTATION_COMPONENT.lowerThreshold,
        MB_CAMERA_MOVEMENT_COMPONENT.lowerThreshold,
        MB_OBJECT_MOVEMENT_COMPONENT.lowerThreshold,
        MB_CAMERA_ROTATION_COMPONENT.upperThreshold,
        MB_CAMERA_MOVEMENT_COMPONENT.upperThreshold,
        MB_OBJECT_MOVEMENT_COMPONENT.upperThreshold,
        temp_intensity,
        0.0f, 0.0f};
    encoder->setUniform(u_mbVelocityData, &mbVelocityData, 3);
    encoder->setUniform(u_previousView, previousView);
    encoder->setUniform(u_previousProj, previousProj);
    encoder->setTexture(0, s_depth, gbufDepth.handle);
    encoder->setTexture(1, s_velocity, gbufVelocity.handle);
    encoder->setImage(2, gMBVelocity.handle, 0, bgfx::Access::Write);
    encoder->setImage(3, gFullVelocity.handle, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_POSTPROCESS, mbVelocityProgram, xGroups, yGroups);

    // Motion blur
    if (settings::enableMotionBlur)
    {
        float mbBlurData[8] = {
            0.0f,
            0.0f,
            MB_MAXIMUM_JITTER_VALUE,
            temp_intensity,
            float(MB_TILE_SIZE),
            float(MB_SAMPLE_COUNT),
            float(mbIndex),
            0.0f};

        if (!freezeTemporalEffects)
        {
            mbIndex = (mbIndex + 1) % 8;
        }

        // Compute tile max
        xGroups = (int)floorf(((float)m_width / MB_TILE_SIZE - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);
        encoder->setUniform(u_mbBlurData, &mbBlurData, 2);
        encoder->setTexture(0, s_velocity, gMBVelocity.handle);
        encoder->setTexture(1, s_depth, gbufDepth.handle);
        encoder->setImage(2, gMBTileMaxX.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, mbGuertinTileMaxXProgram, xGroups, yGroups);

        yGroups = (int)floorf(((float)m_height / MB_TILE_SIZE - 1) / 16 + 1);
        encoder->setUniform(u_mbBlurData, &mbBlurData, 2);
        encoder->setTexture(0, s_mbTileMaxX, gMBTileMaxX.handle);
        encoder->setImage(1, gMBTileMax.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, mbGuertinTileMaxYProgram, xGroups, yGroups);

        // Compute neighbor max
        encoder->setTexture(0, s_mbTileMax, gMBTileMax.handle);
        encoder->setImage(1, gMBNeighborMax.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, mbGuertinNeighborMaxProgram, xGroups, yGroups);

        // Compute tile variance
        encoder->setTexture(0, s_mbTileMax, gMBTileMax.handle);
        encoder->setImage(1, gMBTileVariance.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, mbGuertinTileVarianceProgram, xGroups, yGroups);

        // Compute blur
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setUniform(u_mbBlurData, &mbBlurData, 2);

        encoder->setTexture(0, s_color, gOutputColor.handle);
        encoder->setTexture(1, s_velocity, gMBVelocity.handle);
        encoder->setTexture(2, s_mbNeighborMax, gMBNeighborMax.handle);
        encoder->setTexture(3, s_mbTileVariance, gMBTileVariance.handle);
        encoder->setImage(4, gMBOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, mbGuertinExperimentalBlurProgram, xGroups, yGroups);

        // Copy to output
        encoder->setImage(0, gMBOutputColor.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, blitProgram, xGroups, yGroups);
    }

    if (settings::enableTAA)
    {
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        // TAA resolve
        Texture taaOutput = taaUseBuffer1 ? gTAABuffer1 : gTAABuffer0;

        if (firstTAAFrame)
        {
            encoder->setImage(0, gOutputColor.handle, 0, bgfx::Access::Read);
            encoder->setImage(1, taaOutput.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, blitProgram, xGroups, yGroups);
            firstTAAFrame = false;
        }
        else
        {
            encoder->setImage(0, gFullVelocity.handle, 0, bgfx::Access::Read);
            encoder->setTexture(1, s_depth, gbufDepth.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
            encoder->setImage(2, gOutputColor.handle, 0, bgfx::Access::Read);
            encoder->setTexture(3, s_taaHistory, taaUseBuffer1 ? gTAABuffer0.handle : gTAABuffer1.handle);
            encoder->setImage(4, taaOutput.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, taaResolveProgram, xGroups, yGroups);

            // Copy to output
            encoder->setImage(0, taaOutput.handle, 0, bgfx::Access::Read);
            encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, blitProgram, xGroups, yGroups);
        }
    }

    // Exposure
    float lutParams[4] = {1.0f / static_cast<float>(tonemappingLut.width), 1.0f / static_cast<float>(tonemappingLut.height), static_cast<float>(tonemappingLut.height - 1), powf(2.0f, tonemappingExposure)};
    encoder->setUniform(u_lutParams, lutParams);
    encoder->setImage(0, gOutputColor.handle, 0, bgfx::Access::ReadWrite);
    encoder->dispatch(VIEW_POSTPROCESS, exposureProgram, xGroups, yGroups);

    // Bloom
    if (settings::enableBloom)
    {
        float bloomThresholdField[4] = {bloomThreshold, bloomThreshold - bloomKnee, 2 * bloomKnee, 0.25f * bloomKnee};

        float mipSizeX = (float)m_width / 2;
        float mipSizeY = (float)m_height / 2;

        for (uint8_t i = 0; i < gOutputColor.mipCount - 1; ++i)
        {
            encoder->setUniform(u_bloomThreshold, bloomThresholdField);
            encoder->setTexture(0, s_tex, gOutputColor.handle, 0, 1, i, 1);

            xGroups = (int)floorf((mipSizeX - 1) / 8 + 1);
            yGroups = (int)floorf((mipSizeY - 1) / 8 + 1);

            float bloomTexelSize[4] = {1.0f / mipSizeX, 1.0f / mipSizeY, static_cast<float>(i), 0.0f};
            encoder->setUniform(u_bloomTexelSize, bloomTexelSize);

            encoder->setImage(1, gOutputColor.handle, i + 1, bgfx::Access::Write);

            encoder->dispatch(VIEW_POSTPROCESS, bloomDownscaleProgram, xGroups, yGroups);

            mipSizeX /= 2;
            mipSizeY /= 2;
        }

        float bloomIntensityField[4] = {bloomIntensity, bloomDirtIntensity, 0.0f, 0.0f};

        for (uint8_t i = gOutputColor.mipCount - 1; i >= 1; --i)
        {
            encoder->setUniform(u_bloomIntensity, bloomIntensityField);
            encoder->setTexture(0, s_tex, gOutputColor.handle, 0, 1, i, 1);
            encoder->setTexture(2, s_bloomDirt, bloomDirtMask.handle);

            mipSizeX = std::max(1.0f, floorf(float(m_width) / (1 << (i - 1))));
            mipSizeY = std::max(1.0f, floorf(float(m_height) / (1 << (i - 1))));

            xGroups = (int)floorf((mipSizeX - 1) / 8 + 1);
            yGroups = (int)floorf((mipSizeY - 1) / 8 + 1);

            float bloomTexelSize[4] = {1.0f / mipSizeX, 1.0f / mipSizeY, static_cast<float>(i), 0.0f};
            encoder->setUniform(u_bloomTexelSize, bloomTexelSize);

            encoder->setImage(1, gOutputColor.handle, i - 1, bgfx::Access::ReadWrite);

            encoder->dispatch(VIEW_POSTPROCESS, bloomUpscaleProgram, xGroups, yGroups);
        }
    }

    // Blit and tonemap

    switch (debugView)
    {
    case DebugView::Albedo:
        encoder->setTexture(0, s_tex, gbufAlbedo.handle);
        break;
    case DebugView::Normal:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gbufNormal.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, debugNormalsProgram, xGroups, yGroups);

        encoder->setTexture(0, s_tex, gOutputColor.handle);
        break;
    case DebugView::Emission:
        encoder->setTexture(0, s_tex, gbufEmission.handle);
        break;
    case DebugView::PBRData:
        encoder->setTexture(0, s_tex, gbufPBRData.handle);
        break;
    case DebugView::Velocity:
        encoder->setTexture(0, s_tex, gbufVelocity.handle);
        break;
    case DebugView::Depth:
        encoder->setTexture(0, s_tex, gbufDepth.handle);
        break;
    case DebugView::OITAccum:
        encoder->setTexture(0, s_tex, gAccumTex.handle);
        break;
    case DebugView::OITReveal:
        encoder->setTexture(0, s_tex, gRevealTex.handle);
        break;
    case DebugView::MotionBlurVelocity:
        encoder->setTexture(0, s_tex, gMBVelocity.handle);
        break;
    case DebugView::MotionBlurTileMaxX:
        encoder->setTexture(0, s_tex, gMBTileMaxX.handle);
        break;
    case DebugView::MotionBlurTileMaxY:
        encoder->setTexture(0, s_tex, gMBTileMax.handle);
        break;
    case DebugView::MotionBlurNeighborMax:
        encoder->setTexture(0, s_tex, gMBNeighborMax.handle);
        break;
    case DebugView::MotionBlurTileVariance:
        encoder->setTexture(0, s_tex, gMBTileVariance.handle);
        break;
    case DebugView::GTAOWorkingDepth0:
        encoder->setTexture(0, s_tex, gGTAOWorkingDepth.handle, 0, 1, 0, 1);
        break;
    case DebugView::GTAOWorkingDepth1:
        encoder->setTexture(0, s_tex, gGTAOWorkingDepth.handle, 0, 1, 1, 1);
        break;
    case DebugView::GTAOWorkingDepth2:
        encoder->setTexture(0, s_tex, gGTAOWorkingDepth.handle, 0, 1, 2, 1);
        break;
    case DebugView::GTAOWorkingDepth3:
        encoder->setTexture(0, s_tex, gGTAOWorkingDepth.handle, 0, 1, 3, 1);
        break;
    case DebugView::GTAOWorkingDepth4:
        encoder->setTexture(0, s_tex, gGTAOWorkingDepth.handle, 0, 1, 4, 1);
        break;
    case DebugView::GTAOWorkingAOTermNormals:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOWorkingAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugNormalsProgram, xGroups, yGroups);

        encoder->setTexture(0, s_tex, gOutputColor.handle);
        break;
    case DebugView::GTAOWorkingAOTermVisibility:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOWorkingAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugVisibilityProgram, xGroups, yGroups);

        encoder->setTexture(0, s_tex, gOutputColor.handle);
        break;
    case DebugView::GTAOWorkingEdges:
        encoder->setTexture(0, s_tex, gGTAOWorkingEdges.handle);
        break;
    default:
        encoder->setTexture(0, s_tex, gOutputColor.handle);
        break;
    }
    encoder->setTexture(1, s_lut, tonemappingLut.handle);
    encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    screenSpaceQuad(!bgfx::getCaps()->originBottomLeft, encoder);

    encoder->submit(VIEW_BLIT, tonemapProgram);

    bgfx::end(encoder);

    if (!freezeTemporalEffects)
    {
        previousJitterX = jitterX;
        previousJitterY = jitterY;

        std::memcpy(previousViewProj, viewProj, sizeof(viewProj));
        std::memcpy(previousView, view, sizeof(view));
        std::memcpy(previousProj, proj, sizeof(proj));
        taaUseBuffer1 = !taaUseBuffer1;
    }
}

void field::cleanup()
{
    fmsUI.reset();
    nt::NetworkTableInstance::Destroy(ntInst);

    for (auto &mesh : fieldMeshes)
    {
        mesh.destroy();
    }

    for (auto &gamePiece : gamePieces)
    {
        for (auto &mesh : gamePiece.meshes)
        {
            mesh.destroy();
        }
    }

    for (auto &robot : robots)
    {
        for (auto &mesh : robot.meshes)
        {
            mesh.destroy();
        }

        for (auto &component : robot.components)
        {
            for (auto &mesh : component.meshes)
            {
                mesh.destroy();
            }
        }
    }

    if (bgfx::isValid(u_baseColor))
        bgfx::destroy(u_baseColor);
    if (bgfx::isValid(u_emissionColor))
        bgfx::destroy(u_emissionColor);
    if (bgfx::isValid(u_skyColor))
        bgfx::destroy(u_skyColor);
    if (bgfx::isValid(u_info))
        bgfx::destroy(u_info);
    if (bgfx::isValid(u_previousModelViewProj))
        bgfx::destroy(u_previousModelViewProj);
    if (bgfx::isValid(u_previousView))
        bgfx::destroy(u_previousView);
    if (bgfx::isValid(u_previousProj))
        bgfx::destroy(u_previousProj);
    if (bgfx::isValid(u_jitter))
        bgfx::destroy(u_jitter);
    if (bgfx::isValid(u_pbrData))
        bgfx::destroy(u_pbrData);
    if (bgfx::isValid(u_lightPos))
        bgfx::destroy(u_lightPos);
    if (bgfx::isValid(u_lightColor))
        bgfx::destroy(u_lightColor);

    if (bgfx::isValid(u_mbVelocityData))
        bgfx::destroy(u_mbVelocityData);
    if (bgfx::isValid(u_mbBlurData))
        bgfx::destroy(u_mbBlurData);

    if (bgfx::isValid(u_bloomThreshold))
        bgfx::destroy(u_bloomThreshold);
    if (bgfx::isValid(u_bloomTexelSize))
        bgfx::destroy(u_bloomTexelSize);
    if (bgfx::isValid(u_bloomIntensity))
        bgfx::destroy(u_bloomIntensity);

    if (bgfx::isValid(u_XeGTAOData))
        bgfx::destroy(u_XeGTAOData);

    if (bgfx::isValid(u_lutParams))
        bgfx::destroy(u_lutParams);

    if (bgfx::isValid(programPBR))
        bgfx::destroy(programPBR);
    if (bgfx::isValid(programPBRTextured))
        bgfx::destroy(programPBRTextured);
    if (bgfx::isValid(programPBRInstanced))
        bgfx::destroy(programPBRInstanced);
    if (bgfx::isValid(programOit))
        bgfx::destroy(programOit);
    if (bgfx::isValid(programOitInstanced))
        bgfx::destroy(programOitInstanced);
    if (bgfx::isValid(programOitDepthPostPass))
        bgfx::destroy(programOitDepthPostPass);
    if (bgfx::isValid(programOitDepthPostPassInstanced))
        bgfx::destroy(programOitDepthPostPassInstanced);
    if (bgfx::isValid(oitCompProgram))
        bgfx::destroy(oitCompProgram);
    if (bgfx::isValid(tonemapProgram))
        bgfx::destroy(tonemapProgram);
    if (bgfx::isValid(blitProgram))
        bgfx::destroy(blitProgram);
    if (bgfx::isValid(debugNormalsProgram))
        bgfx::destroy(debugNormalsProgram);
    if (bgfx::isValid(exposureProgram))
        bgfx::destroy(exposureProgram);
    if (bgfx::isValid(taaResolveProgram))
        bgfx::destroy(taaResolveProgram);
    if (bgfx::isValid(mbVelocityProgram))
        bgfx::destroy(mbVelocityProgram);
    if (bgfx::isValid(mbGuertinTileMaxXProgram))
        bgfx::destroy(mbGuertinTileMaxXProgram);
    if (bgfx::isValid(mbGuertinTileMaxYProgram))
        bgfx::destroy(mbGuertinTileMaxYProgram);
    if (bgfx::isValid(mbGuertinNeighborMaxProgram))
        bgfx::destroy(mbGuertinNeighborMaxProgram);
    if (bgfx::isValid(mbGuertinTileVarianceProgram))
        bgfx::destroy(mbGuertinTileVarianceProgram);
    if (bgfx::isValid(mbGuertinExperimentalBlurProgram))
        bgfx::destroy(mbGuertinExperimentalBlurProgram);
    if (bgfx::isValid(bloomDownscaleProgram))
        bgfx::destroy(bloomDownscaleProgram);
    if (bgfx::isValid(bloomUpscaleProgram))
        bgfx::destroy(bloomUpscaleProgram);
    if (bgfx::isValid(XeGTAO_PrefilterDepths16x16Program))
        bgfx::destroy(XeGTAO_PrefilterDepths16x16Program);
    if (bgfx::isValid(XeGTAO_MainPassProgram))
        bgfx::destroy(XeGTAO_MainPassProgram);
    if (bgfx::isValid(XeGTAO_debugNormalsProgram))
        bgfx::destroy(XeGTAO_debugNormalsProgram);
    if (bgfx::isValid(XeGTAO_debugVisibilityProgram))
        bgfx::destroy(XeGTAO_debugVisibilityProgram);

    gAccumTex.destroy();
    gRevealTex.destroy();

    gOitFbo.destroy();
    gOitDepthPostPassFbo.destroy();

    gbufAlbedo.destroy();
    gbufEmission.destroy();
    gbufNormal.destroy();
    gbufPBRData.destroy();
    gbufVelocity.destroy();
    gFullVelocity.destroy();
    gbufDepth.destroy();

    gBufFbo.destroy();

    gOutputColor.destroy();

    gTAABuffer0.destroy();
    gTAABuffer1.destroy();

    gMBTileMaxX.destroy();
    gMBTileMax.destroy();
    gMBNeighborMax.destroy();
    gMBTileVariance.destroy();
    gMBOutputColor.destroy();
    gMBVelocity.destroy();

    gGTAOWorkingDepth.destroy();
    gGTAOWorkingAOTerm.destroy();
    gGTAOWorkingEdges.destroy();
    gGTAOHilbertLut.destroy();

    bloomDirtMask.destroy();

    tonemappingLut.destroy();

    carpetBaseColor.destroy();
    carpetBump.destroy();

    if (bgfx::isValid(s_tex))
        bgfx::destroy(s_tex);
    if (bgfx::isValid(s_taaHistory))
        bgfx::destroy(s_taaHistory);

    if (bgfx::isValid(s_velocity))
        bgfx::destroy(s_velocity);
    if (bgfx::isValid(s_depth))
        bgfx::destroy(s_depth);
    if (bgfx::isValid(s_color))
        bgfx::destroy(s_color);
    if (bgfx::isValid(s_mbTileMaxX))
        bgfx::destroy(s_mbTileMaxX);
    if (bgfx::isValid(s_mbTileMax))
        bgfx::destroy(s_mbTileMax);
    if (bgfx::isValid(s_mbNeighborMax))
        bgfx::destroy(s_mbNeighborMax);
    if (bgfx::isValid(s_mbTileVariance))
        bgfx::destroy(s_mbTileVariance);

    if (bgfx::isValid(s_bloomDirt))
        bgfx::destroy(s_bloomDirt);
    if (bgfx::isValid(s_lut))
        bgfx::destroy(s_lut);

    if (bgfx::isValid(s_baseColor))
        bgfx::destroy(s_baseColor);
    if (bgfx::isValid(s_bump))
        bgfx::destroy(s_bump);
}