#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <SDL3/SDL.h>
#include <bx/file.h>
#include <bx/error.h>
#include <bx/pixelformat.h>
#include <imgui/imgui.h>

#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>
#include <random>

#include <nlohmann/json.hpp>

#include <blackboard_app/logger.h>

#include "texture.h"

#include "fieldrenderer.h"
#include "shaders.h"
#include "mesh.h"
#include <future>

#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>
#include <networktables/StructTopic.h>
#include <networktables/StructArrayTopic.h>

#include <frc/geometry/Pose3d.h>
#include <frc/geometry/struct/Pose3dStruct.h>

#include "../settings/settingsstore.h"

#include <bloom_dirt_mask.png.h>

#if GAME_YEAR == 2026
#include "seasonspecific/rebuilt2026/fmsui.h"
#include "seasonspecific/rebuilt2026/hublights.h"
#endif


static const bgfx::EmbeddedShader s_embeddedShaders[] =
    {
        BGFX_EMBEDDED_SHADER(vs_pbr),
        BGFX_EMBEDDED_SHADER(vs_pbr_instanced),
        BGFX_EMBEDDED_SHADER(fs_pbr),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit_depth_post_pass),

        BGFX_EMBEDDED_SHADER(vs_pass),
        BGFX_EMBEDDED_SHADER(fs_tonemap),

        BGFX_EMBEDDED_SHADER(cs_blit),
        BGFX_EMBEDDED_SHADER(cs_oit_comp),
        BGFX_EMBEDDED_SHADER(cs_taa_resolve),
        BGFX_EMBEDDED_SHADER(cs_mb_velocity),
        BGFX_EMBEDDED_SHADER(cs_mb_tilemax_x),
        BGFX_EMBEDDED_SHADER(cs_mb_tilemax_y),
        BGFX_EMBEDDED_SHADER(cs_mb_jfa),
        BGFX_EMBEDDED_SHADER(cs_mb_jfa_backtracking),
        BGFX_EMBEDDED_SHADER(cs_mb_neighbormax),
        BGFX_EMBEDDED_SHADER(cs_mb_blur),
        BGFX_EMBEDDED_SHADER(cs_mb_blur_simple),
        BGFX_EMBEDDED_SHADER(cs_mb_cache),

        BGFX_EMBEDDED_SHADER(cs_bloom_downscale),
        BGFX_EMBEDDED_SHADER(cs_bloom_upscale),

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
    RobotRelative
};
#define CAMERA_VIEW_COUNT 3

static const std::array<std::string, CAMERA_VIEW_COUNT> CAMERA_VIEW_NAMES = {
    "Field",
    "Robot",
    "Robot Relative"};

static constexpr float INCHES_TO_METERS = 0.0254f;

static constexpr uint16_t MB_SAMPLE_STEP_MULTIPLIER = 16;
static constexpr float MB_PERPEN_ERROR_THRESHOLD = 0.3f;
static constexpr float MB_STEP_EXPONENT_MODIFIER = 1.3f;
static constexpr uint16_t MB_BACKTRACKING_SAMPLE_COUNT = 8;
static constexpr float MB_BACKTRACKING_VELOCITY_MATCH_THRESHOLD = 0.9f;
static constexpr float MB_BACKTRACKING_VELOCITY_PARALLEL_S = 1.0f;
static constexpr float MB_BACKTRACKING_VELOCITY_PERPENDICULAR_S = 0.05f;
static constexpr float MB_BACKTRACKING_DEPTH_MATCH_THRESHOLD = 0.001f;
static constexpr uint16_t MB_JFA_PASS_COUNT = 3;
static constexpr bool MB_FRAMERATE_INDEPENDENT = true;
static constexpr bool MB_UNCAPPED_INDEPENDENCE = false;
static constexpr float MB_TARGET_CONSTANT_FRAMERATE = 30;

static constexpr bool SCALE_MB_BUFFERS = false;

static constexpr uint8_t BLOOM_DOWNSCALE_LIMIT = 10;
static constexpr uint8_t BLOOM_MAX_ITERATIONS = 16;

static uint8_t calculateBloomMipmapLevels(uint16_t width, uint16_t height)
{
    width  /= 2;
    height /= 2;
    uint8_t  mip_levels = 1;

    for (uint8_t i = 0; i < BLOOM_MAX_ITERATIONS; ++i)
    {
        width /= 2;
        height /= 2;

        if (width < BLOOM_DOWNSCALE_LIMIT || height < BLOOM_DOWNSCALE_LIMIT) break;

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
static constexpr MBVelocityComponent MB_OBJECT_MOVEMENT_COMPONENT = {1.0f, 0.0f, 1.0f};

using namespace blackboard::logger;

static constexpr uint16_t VIEW_GBUFFER = 0;
static constexpr uint16_t VIEW_OIT = 1;
static constexpr uint16_t VIEW_OIT_DEPTH_POST_PASS = 2;
static constexpr uint16_t VIEW_POSTPROCESS = 3;
static constexpr uint16_t VIEW_BLIT = 4;

bgfx::UniformHandle u_baseColor;
bgfx::UniformHandle u_emissionColor;
bgfx::UniformHandle u_info;
bgfx::UniformHandle u_normalMatrix;
bgfx::UniformHandle u_previousModelViewProj;
bgfx::UniformHandle u_previousView;
bgfx::UniformHandle u_previousProj;
bgfx::UniformHandle u_jitter;
bgfx::UniformHandle u_pbrData;

bgfx::UniformHandle u_lightPos;
bgfx::UniformHandle u_lightColor;

bgfx::UniformHandle u_mbSampleStepMultiplier;
bgfx::UniformHandle u_mbVelocityData;
bgfx::UniformHandle u_mbJFAData;
bgfx::UniformHandle u_mbBlurData;

bgfx::UniformHandle u_bloomThreshold;
bgfx::UniformHandle u_bloomTexelSize;
bgfx::UniformHandle u_bloomIntensity;

bgfx::ProgramHandle programPBR = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programPBRInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOit = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitDepthPostPass = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitDepthPostPassInstanced = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle oitCompProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle tonemapProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle blitProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle taaResolveProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle mbVelocityProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbTileMaxXProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbTileMaxYProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbJFAProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbJFABacktrackingProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbNeighborMaxProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbBlurProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbBlurSimpleProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle mbCacheProgram = BGFX_INVALID_HANDLE;

bgfx::ProgramHandle bloomDownscaleProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle bloomUpscaleProgram = BGFX_INVALID_HANDLE;

Texture gAccumTex;
Texture gRevealTex;

Texture gbufAlbedo;
Texture gbufEmission;
Texture gbufNormal;
Texture gbufVelocity;
Texture gFullVelocity;
Texture gMBPreviousVelocity;
Texture gbufDepth;

Texture gOutputColor;
Texture gMBPreviousOutputColor;

Texture gTAABuffer0;
Texture gTAABuffer1;

Texture gMBTileMaxX;
Texture gMBTileMax;
Texture gMBNeighborMax;
Texture gMBBufferA;
Texture gMBBufferB;
Texture gMBVelocity;
Texture gMBOutputColor;

Texture bloomDirtMask;

FrameBuffer gBufFbo;
FrameBuffer gOitFbo;
FrameBuffer gOitDepthPostPassFbo;

bgfx::UniformHandle s_tex;
bgfx::UniformHandle s_taaHistory;

bgfx::UniformHandle s_color;
bgfx::UniformHandle s_velocity;
bgfx::UniformHandle s_depth;
bgfx::UniformHandle s_prevColor;
bgfx::UniformHandle s_prevVelocity;
bgfx::UniformHandle s_mbTileMaxX;
bgfx::UniformHandle s_mbTileMax;
bgfx::UniformHandle s_mbNeighborMax;
bgfx::UniformHandle s_mbBuffer;
bgfx::UniformHandle s_bloomDirt;

float bloomThreshold = 5.2f;
float bloomKnee = 0.1f;
float bloomIntensity = 1.0f;
float bloomDirtIntensity = 1.1f;

float fieldModelMatrix[16];
float fieldNormalMatrix[9];

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

bool freezeTemporalEffects = false;

void initPBROIT(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    programPBR =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr"), true);

    if (!bgfx::isValid(programPBR))
    {
        logger->error("Failed to create PBR rendering program.");
        throw std::runtime_error("Failed to create PBR rendering program.");
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

    u_info = bgfx::createUniform("u_info", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_info))
    {
        logger->error("Failed to create uniform: u_info");
        throw std::runtime_error("Failed to create uniform: u_info");
    }

    u_normalMatrix = bgfx::createUniform("u_normalMatrix", bgfx::UniformType::Mat3);
    if (!bgfx::isValid(u_normalMatrix))
    {
        logger->error("Failed to create uniform: u_normalMatrix");
        throw std::runtime_error("Failed to create uniform: u_normalMatrix");
    }

    u_pbrData = bgfx::createUniform("u_pbrData", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_pbrData))
    {
        logger->error("Failed to create uniform: u_pbrData");
        throw std::runtime_error("Failed to create uniform: u_pbrData");
    }

    u_lightPos = bgfx::createUniform("u_lightPos", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_lightPos))
    {
        logger->error("Failed to create uniform: u_lightPos");
        throw std::runtime_error("Failed to create uniform: u_lightPos");
    }

    u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_lightColor))
    {
        logger->error("Failed to create uniform: u_lightColor");
        throw std::runtime_error("Failed to create uniform: u_lightColor");
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
}

void initTonemap()
{
    const auto type = bgfx::getRendererType();

    tonemapProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pass"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_tonemap"), true);

    if (!bgfx::isValid(tonemapProgram))
    {
        logger->error("Failed to create tonemap program.");
        throw std::runtime_error("Failed to create tonemap program.");
    }
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

    taaResolveProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_taa_resolve"), true);

    if (!bgfx::isValid(taaResolveProgram))
    {
        logger->error("Failed to create TAA resolve program.");
        throw std::runtime_error("Failed to create TAA resolve program.");
    }

    u_jitter = bgfx::createUniform("u_jitter", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_jitter))
    {
        logger->error("Failed to create uniform: u_jitter");
        throw std::runtime_error("Failed to create uniform: u_jitter");
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
        bgfx::TextureFormat::RGBA8S,
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
        bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    FRAMEBUFFER(
        gBufFbo,
        &gbufAlbedo, &gbufEmission, &gbufNormal, &gbufVelocity, &gbufDepth);
}

void initMotionBlur(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_prevColor = bgfx::createUniform("s_prevColor", bgfx::UniformType::Sampler);
    s_prevVelocity = bgfx::createUniform("s_prevVelocity", bgfx::UniformType::Sampler);
    s_mbTileMaxX = bgfx::createUniform("s_tilemax_x", bgfx::UniformType::Sampler);
    s_mbTileMax = bgfx::createUniform("s_tilemax", bgfx::UniformType::Sampler);
    s_mbNeighborMax = bgfx::createUniform("s_neighbormax", bgfx::UniformType::Sampler);
    s_mbBuffer = bgfx::createUniform("s_buffer", bgfx::UniformType::Sampler);

    mbVelocityProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_velocity"), true);
    if (!bgfx::isValid(mbVelocityProgram))
    {
        logger->error("Failed to create motion blur velocity program.");
        throw std::runtime_error("Failed to create motion blur velocity program.");
    }

    mbTileMaxXProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_tilemax_x"), true);
    if (!bgfx::isValid(mbTileMaxXProgram))
    {
        logger->error("Failed to create motion blur tile max X program.");
        throw std::runtime_error("Failed to create motion blur tile max X program.");
    }

    mbTileMaxYProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_tilemax_y"), true);
    if (!bgfx::isValid(mbTileMaxYProgram))
    {
        logger->error("Failed to create motion blur tile max Y program.");
        throw std::runtime_error("Failed to create motion blur tile max Y program.");
    }

    mbJFAProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_jfa"), true);
    if (!bgfx::isValid(mbJFAProgram))
    {
        logger->error("Failed to create motion blur JFA program.");
        throw std::runtime_error("Failed to create motion blur JFA program.");
    }

    mbJFABacktrackingProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_jfa_backtracking"), true);
    if (!bgfx::isValid(mbJFABacktrackingProgram))
    {
        logger->error("Failed to create motion blur JFA backtracking program.");
        throw std::runtime_error("Failed to create motion blur JFA backtracking program.");
    }

    mbNeighborMaxProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_neighbormax"), true);
    if (!bgfx::isValid(mbNeighborMaxProgram))
    {
        logger->error("Failed to create motion blur neighbor max program.");
        throw std::runtime_error("Failed to create motion blur neighbor max program.");
    }

    mbBlurProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_blur"), true);
    if (!bgfx::isValid(mbBlurProgram))
    {
        logger->error("Failed to create motion blur blur program.");
        throw std::runtime_error("Failed to create motion blur blur program.");
    }

    mbBlurSimpleProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_blur_simple"), true);
    if (!bgfx::isValid(mbBlurSimpleProgram))
    {
        logger->error("Failed to create motion blur simple blur program.");
        throw std::runtime_error("Failed to create motion blur simple blur program.");
    }

    mbCacheProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_mb_cache"), true);
    if (!bgfx::isValid(mbCacheProgram))
    {
        logger->error("Failed to create motion blur cache program.");
        throw std::runtime_error("Failed to create motion blur cache program.");
    }

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

    u_mbSampleStepMultiplier = bgfx::createUniform("u_mbSampleStepMultiplier", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_mbSampleStepMultiplier))
    {
        logger->error("Failed to create uniform: u_mbSampleStepMultiplier");
        throw std::runtime_error("Failed to create uniform: u_mbSampleStepMultiplier");
    }

    u_mbVelocityData = bgfx::createUniform("u_mbVelocityData", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_mbVelocityData))
    {
        logger->error("Failed to create uniform: u_mbVelocityData");
        throw std::runtime_error("Failed to create uniform: u_mbVelocityData");
    }

    u_mbJFAData = bgfx::createUniform("u_mbJFAData", bgfx::UniformType::Vec4, 2);
    if (!bgfx::isValid(u_mbJFAData))
    {
        logger->error("Failed to create uniform: u_mbJFAData");
        throw std::runtime_error("Failed to create uniform: u_mbJFAData");
    }

    u_mbBlurData = bgfx::createUniform("u_mbBlurData", bgfx::UniformType::Vec4, 2);
    if (!bgfx::isValid(u_mbBlurData))
    {
        logger->error("Failed to create uniform: u_mbBlurData");
        throw std::runtime_error("Failed to create uniform: u_mbBlurData");
    }

    TEXTURE(
        gMBTileMaxX,
        width,
        height,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBTileMax,
        width,
        height,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBNeighborMax,
        width,
        height,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBBufferA,
        width,
        height,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBBufferB,
        width,
        height,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
        SCALE_MB_BUFFERS ? float(1.0f / MB_SAMPLE_STEP_MULTIPLIER) : 1.0f,
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

    TEXTURE(
        gMBPreviousVelocity,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBPreviousOutputColor,
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

    s_bloomDirt = bgfx::createUniform("s_bloomDirt", bgfx::UniformType::Sampler);
    if(!bgfx::isValid(s_bloomDirt))
    {
        logger->error("Failed to create uniform: s_bloomDirt");
        throw std::runtime_error("Failed to create uniform: s_bloomDirt");
    }

    u_bloomThreshold = bgfx::createUniform("u_threshold", bgfx::UniformType::Vec4);
    if(!bgfx::isValid(u_bloomThreshold))
    {
        logger->error("Failed to create uniform: u_bloomThreshold");
        throw std::runtime_error("Failed to create uniform: u_bloomThreshold");
    }
    u_bloomTexelSize = bgfx::createUniform("u_texel_size", bgfx::UniformType::Vec4);
    if(!bgfx::isValid(u_bloomTexelSize))
    {
        logger->error("Failed to create uniform: u_bloomTexelSize");
        throw std::runtime_error("Failed to create uniform: u_bloomTexelSize");
    }
    u_bloomIntensity = bgfx::createUniform("u_bloom_intensity", bgfx::UniformType::Vec4);
    if(!bgfx::isValid(u_bloomIntensity))
    {
        logger->error("Failed to create uniform: u_bloomIntensity");
        throw std::runtime_error("Failed to create uniform: u_bloomIntensity");
    }

    TEXTURE_EMBEDDED(
        bloomDirtMask,
        bloom_dirt_mask_png,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
}

typedef struct Transform
{
    float matrix[16];

    Transform()
    {
        bx::mtxIdentity(matrix);
    }

    Transform(float *modelMatrix, bx::Vec3 position, bx::Vec3 rotation)
    {
        float rotationMatrix[16];
        bx::mtxRotateXYZ(rotationMatrix, rotation.x, rotation.y, rotation.z);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
        float relativeMatrix[16];
        bx::mtxMul(relativeMatrix, rotationMatrix, translationMatrix);
        bx::mtxMul(matrix, modelMatrix, relativeMatrix);
    }

    Transform(float *modelMatrix, float *parentMatrix, bx::Vec3 position, bx::Vec3 rotation)
    {
        float rotationMatrix[16];
        bx::mtxRotateXYZ(rotationMatrix, rotation.x, rotation.y, rotation.z);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
        float relativeMatrix[16];
        bx::mtxMul(relativeMatrix, rotationMatrix, translationMatrix);
        float finalMatrix[16];
        bx::mtxMul(finalMatrix, modelMatrix, relativeMatrix);
        bx::mtxMul(matrix, finalMatrix, parentMatrix);
    }

    Transform(bx::Vec3 position, bx::Vec3 rotation)
    {
        float rotationMatrix[16];
        bx::mtxRotateXYZ(rotationMatrix, rotation.x, rotation.y, rotation.z);
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
    bool hasData = false;
    bx::Vec3 position = {0.0f, 0.0f, 0.0f};
    bx::Vec3 rotation = {0.0f, 0.0f, 0.0f};

    InstanceData instanceData = {};

    void update(float *modelMatrix, float deltaTime)
    {
        if (!freezeTemporalEffects)
        {
            instanceData.previousTransform = instanceData.transform;
        }

        instanceData.transform = {
            modelMatrix,
            position,
            rotation};
    }

    void update(float *modelMatrix, float *parentMatrix, float deltaTime)
    {
        if (!freezeTemporalEffects)
        {
            instanceData.previousTransform = instanceData.transform;
        }

        instanceData.transform = {
            modelMatrix,
            parentMatrix,
            position,
            rotation};
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
    std::vector<DynamicObjectData> instances;
    std::array<float, 16> modelMatrix;
    std::vector<Mesh> meshes;

    nt::StructArrayTopic<frc::Pose3d> posesTopic;
    nt::StructArraySubscriber<frc::Pose3d> posesSub;
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

static void toMat3(float m3[9], const float m4[16])
{
    m3[0] = m4[0];
    m3[1] = m4[1];
    m3[2] = m4[2];

    m3[3] = m4[4];
    m3[4] = m4[5];
    m3[5] = m4[6];

    m3[6] = m4[8];
    m3[7] = m4[9];
    m3[8] = m4[10];
}

static void mtx3Transpose(float out[9], const float in[9])
{
    out[0] = in[0];
    out[1] = in[3];
    out[2] = in[6];

    out[3] = in[1];
    out[4] = in[4];
    out[5] = in[7];

    out[6] = in[2];
    out[7] = in[5];
    out[8] = in[8];
}

static void toNormalMatrix(float normalMatrix[9], const float model[16])
{
    float mat3[9];
    toMat3(mat3, model);
    float inverse[9];
    bx::mtx3Inverse(inverse, mat3);
    mtx3Transpose(normalMatrix, inverse);
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
            frc::Rotation3d(
                -pose.Rotation().X(),
                -pose.Rotation().Y(),
                units::degree_t{180} - pose.Rotation().Z()));
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
        toNormalMatrix(fieldNormalMatrix, fieldModelMatrix);

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
            for(auto &mesh : gamePieces[i].meshes)
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
        for(auto &mesh : robots.back().meshes)
        {
            mesh.material.writesObjectMotionVectors = true;
        }
        for (size_t i = 0; i < components.size(); i++)
        {
            loadAndCacheMeshes(robots.back().components[i].meshes, robotDirectory, "model_" + std::to_string(i), {});
            for(auto &mesh : robots.back().components[i].meshes)
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

    initGBuffer(window.width, window.height);
    initPBROIT(window.width, window.height);
    initTonemap();
    initTAA(window.width, window.height);
    initMotionBlur(window.width, window.height);
    initBloom(window.width, window.height);
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

void updateInfo(bgfx::Encoder *encoder, float cameraNear, float cameraFar)
{
    float info[4] = {cameraNear, cameraFar, 0.0f, 0.0f};
    encoder->setUniform(u_info, info);
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
int jitterIndex = 0;

int mbIndex = 0;

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
    gbufVelocity.beginFrame();
    gFullVelocity.beginFrame();
    gMBPreviousVelocity.beginFrame();
    gbufDepth.beginFrame();

    gOutputColor.beginFrame();
    gMBPreviousOutputColor.beginFrame();

    gTAABuffer0.beginFrame();
    gTAABuffer1.beginFrame();

    gMBTileMaxX.beginFrame();
    gMBTileMax.beginFrame();
    gMBNeighborMax.beginFrame();
    gMBBufferA.beginFrame();
    gMBBufferB.beginFrame();
    gMBOutputColor.beginFrame();
    gMBVelocity.beginFrame();

    gAccumTex.ensure(width, height);
    gRevealTex.ensure(width, height);

    gOitFbo.ensure(width, height);
    gOitDepthPostPassFbo.ensure(width, height);

    gbufAlbedo.ensure(width, height);
    gbufEmission.ensure(width, height);
    gbufNormal.ensure(width, height);
    gbufVelocity.ensure(width, height);
    gFullVelocity.ensure(width, height);
    gMBPreviousVelocity.ensure(width, height);
    gbufDepth.ensure(width, height);

    gBufFbo.ensure(width, height);

    gOutputColor.ensure(width, height);
    gOutputColor.mipCount = calculateBloomMipmapLevels(gOutputColor.width, gOutputColor.height);
    gMBPreviousOutputColor.ensure(width, height);

    gTAABuffer0.ensure(width, height);
    gTAABuffer1.ensure(width, height);

    gMBTileMaxX.ensure(width, height);
    gMBTileMax.ensure(width, height);
    gMBNeighborMax.ensure(width, height);
    gMBBufferA.ensure(width, height);
    gMBBufferB.ensure(width, height);
    gMBOutputColor.ensure(width, height);
    gMBVelocity.ensure(width, height);
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

    encoder->setUniform(u_baseColor, mesh.material.baseColor.data());
    encoder->setUniform(u_emissionColor, mesh.material.emissionColor.data());
    encoder->setUniform(u_previousModelViewProj, previousViewProj);
    encoder->setUniform(u_pbrData, pbrData);
}

template <std::ranges::input_range R>
void drawMeshes(bgfx::Encoder *encoder, R &&meshes, float modelMatrix[16], float normalMatrix[9])
{
    for (const auto &mesh : meshes)
    {
        setupMesh(encoder, mesh, false);
        encoder->setTransform(modelMatrix);
        encoder->setUniform(u_normalMatrix, normalMatrix);
        if (mesh.material.type == MaterialType::Transparent)
        {
            encoder->submit(VIEW_OIT, programOit);

#if 0 // IF TRANSPARENT MOTION VECTORS AND DEPTH (transparent TAA)
            setupMesh(encoder, mesh, true);
            encoder->setTransform(modelMatrix);
            encoder->setUniform(u_normalMatrix, normalMatrix);
            encoder->submit(VIEW_OIT_DEPTH_POST_PASS, programOitDepthPostPass);
#endif
        }
        else
        {
            encoder->submit(VIEW_GBUFFER, programPBR);
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

void field::render(const blackboard::app::Window &window)
{
    ImGui::Begin("Options");
    ImGui::Checkbox("Freeze Temporal Effects", &freezeTemporalEffects);
    ImGui::Checkbox("Write Object Motion Vectors", &settings::writeObjectMotionVectors);
    ImGui::Checkbox("Enable Motion Blur", &settings::enableMotionBlur);
    ImGui::Checkbox("Enable TAA", &settings::enableTAA);
    ImGui::Checkbox("Enable Bloom", &settings::enableBloom);

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
            robot.dynamicData.hasData = true;

            frc::Pose3d localPose = transformPose3dToLocalCoordinates(robotPose.value);
            robot.dynamicData.position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
            robot.dynamicData.rotation = {static_cast<float>(localPose.Rotation().X().value()), static_cast<float>(localPose.Rotation().Y().value()), static_cast<float>(localPose.Rotation().Z().value())};
            robot.dynamicData.update(robot.modelMatrix.data(), deltaTime);

            // Update component poses
            auto componentPoses = robot.componentPosesSub.GetAtomic();
            Transform robotOrigin{robot.dynamicData.position, robot.dynamicData.rotation};
            for (size_t i = 0; i < robot.components.size(); ++i)
            {
                robot.components[i].dynamicData.hasData = true;

                if (robot.componentPosesSub.Exists() && i < componentPoses.value.size())
                {
                    // invert rotation to match advscope
                    robot.components[i].dynamicData.position = {static_cast<float>(componentPoses.value[i].X().value()), static_cast<float>(componentPoses.value[i].Y().value()), static_cast<float>(componentPoses.value[i].Z().value())};
                    robot.components[i].dynamicData.rotation = {-static_cast<float>(componentPoses.value[i].Rotation().X().value()), -static_cast<float>(componentPoses.value[i].Rotation().Y().value()), -static_cast<float>(componentPoses.value[i].Rotation().Z().value())};
                    robot.components[i].dynamicData.update(robot.components[i].modelMatrix.data(), robotOrigin.matrix, deltaTime);
                }
                else
                {
                    robot.components[i].dynamicData.position = {0.0f, 0.0f, 0.0f};
                    robot.components[i].dynamicData.rotation = {0.0f, 0.0f, 0.0f};
                    robot.components[i].dynamicData.update(robot.modelMatrix.data(), robotOrigin.matrix, deltaTime);
                }
            }
        }
        else
        {
            robot.dynamicData.hasData = false;
            for (auto &component : robot.components)
            {
                component.dynamicData.hasData = false;
            }
        }
    }

    for (auto &gamePiece : gamePieces)
    {
        auto gamePiecePoses = gamePiece.posesSub.GetAtomic();
        if (gamePiece.posesSub.Exists())
        {
            for (size_t i = 0; i < std::max(gamePiecePoses.value.size(), gamePiece.instances.size()); ++i)
            {
                if (i >= gamePiece.instances.size())
                {
                    gamePiece.instances.emplace_back();
                }
                else if (i >= gamePiecePoses.value.size())
                {
                    gamePiece.instances[i].hasData = false;
                    continue;
                }

                gamePiece.instances[i].hasData = true;

                auto localPose = transformPose3dToLocalCoordinates(gamePiecePoses.value[i]);
                gamePiece.instances[i].position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
                gamePiece.instances[i].rotation = {static_cast<float>(localPose.Rotation().X().value()), static_cast<float>(localPose.Rotation().Y().value()), static_cast<float>(localPose.Rotation().Z().value())};
                gamePiece.instances[i].update(gamePiece.modelMatrix.data(), deltaTime);
            }
        }
        else
        {
            for (auto &instance : gamePiece.instances)
            {
                instance.hasData = false;
            }
        }
    }

    if (cameraView == CameraView::Robot || cameraView == CameraView::RobotRelative)
    {
        if (robots.size() > 0 && robots.back().dynamicData.hasData)
        {
            float translationMtx[16];
            bx::mtxTranslate(translationMtx, robots.back().dynamicData.position.x, robots.back().dynamicData.position.y, robots.back().dynamicData.position.z);
            if (cameraView == CameraView::RobotRelative)
            {
                // also apply rotation
                float rotationMtx[16];
                bx::mtxRotateXYZ(rotationMtx, robots.back().dynamicData.rotation.x, robots.back().dynamicData.rotation.y, robots.back().dynamicData.rotation.z);
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

    updateInfo(encoder, 0.1f, 100.0f);

    if (!freezeTemporalEffects)
    {
        jitterIndex = (jitterIndex + 1) % HALTON_SAMPLES;
    }

    float jitter[4] = {jitterX, jitterY, previousJitterX, previousJitterY};
    encoder->setUniform(u_jitter, jitter);

    // Light uniforms
    float lightPos[3][4] = {
        {-fieldWidthMeters / 2.3f, 0.0f, 6.0f, 0.0f},
        {0.0f, 0.0f, 6.0f, 0.0f},
        {fieldWidthMeters / 2.3f, 0.0f, 6.0f, 0.0f}};
    float lightColor[3][4] = {
        {1.0f, 0.25f, 0.25f, 432.0f},
        {1.0f, 1.0f, 1.0f, 432.0f},
        {0.25f, 0.45f, 1.0f, 432.0f}};
    encoder->setUniform(u_lightPos, lightPos, 3);
    encoder->setUniform(u_lightColor, lightColor, 3);

    // OPAQUE PASS
    bgfx::setViewName(VIEW_GBUFFER, "Field - GBuffer");
    bgfx::setViewTransform(VIEW_GBUFFER, view, proj);
    bgfx::setViewRect(VIEW_GBUFFER, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_GBUFFER, gBufFbo.handle);
    bgfx::setViewClear(VIEW_GBUFFER,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000,
                       bgfx::getCaps()->homogeneousDepth ? -1.0f : 0.0f);

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

    if (createdFieldMeshBuffers)
    {
        std::vector<std::string> drawnGamePieces;
        for (auto &gamePiece : gamePieces)
        {
            std::vector<InstanceData> instanceData;
            auto filtered = gamePiece.instances | std::views::filter([](const DynamicObjectData &dynamicData)
                                                                     { return dynamicData.hasData; });
            instanceData.reserve(std::ranges::distance(filtered));
            std::ranges::transform(filtered, std::back_inserter(instanceData),
                                   [](const DynamicObjectData &dynamicData)
                                   { return dynamicData.instanceData; });
            if (!instanceData.empty())
            {
                drawMeshesInstanced(encoder, gamePiece.meshes, instanceData);
                drawnGamePieces.push_back(gamePiece.name);
            }
        }

        drawMeshes(encoder, fieldMeshes | std::views::filter([&drawnGamePieces](const Mesh &mesh)
                                                             { return std::find(drawnGamePieces.begin(), drawnGamePieces.end(), mesh.tag) == drawnGamePieces.end(); /* only draw meshes that haven't been drawn by game pieces */ }),
                   fieldModelMatrix, fieldNormalMatrix);
    }

    if (createdRobotMeshBuffers)
    {
        std::vector<InstanceData> instanceData;
        auto filtered = robots | std::views::filter([](const RobotData &robot)
                                                    { return robot.dynamicData.hasData; });
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
            if (!component.dynamicData.hasData)
            {
                continue;
            }

            std::vector<InstanceData> componentInstanceData = {
                component.dynamicData.instanceData};
            drawMeshesInstanced(encoder, component.meshes, componentInstanceData);
        }
    }

#if 0 // IF NO TRANSPARENT OBJECTS IN FIELD MODEL (RARE)
        encoder->touch(VIEW_OIT); // needs to be cleared
#endif

    // Post processing
    bgfx::setViewName(VIEW_POSTPROCESS, "Field - Post Process");
    bgfx::setViewMode(VIEW_POSTPROCESS, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(VIEW_POSTPROCESS, view, proj);
    bgfx::setViewRect(VIEW_POSTPROCESS, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_POSTPROCESS, BGFX_INVALID_HANDLE);

    encoder->setUniform(u_previousModelViewProj, previousViewProj);

    int xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    int yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

    // OIT Composition
    encoder->setImage(0, gAccumTex.handle, 0, bgfx::Access::Read);
    encoder->setImage(1, gRevealTex.handle, 0, bgfx::Access::Read);
    encoder->setImage(2, gbufAlbedo.handle, 0, bgfx::Access::Read);
    encoder->setImage(3, gbufEmission.handle, 0, bgfx::Access::Read);
    encoder->setImage(4, gbufNormal.handle, 0, bgfx::Access::Read);
    encoder->setImage(5, gbufDepth.handle, 0, bgfx::Access::Read);
    encoder->setImage(6, gOutputColor.handle, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_POSTPROCESS, oitCompProgram, xGroups, yGroups);

    // Motion blur Velocity

    float samples = 16.0f;
    float centerFade = 0.0f;

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

    uint16_t lastIterationIndex = MB_JFA_PASS_COUNT - 1;
    float maxDilationRadius = powf(2 + MB_STEP_EXPONENT_MODIFIER, lastIterationIndex) * MB_SAMPLE_STEP_MULTIPLIER / intensity;
    float sampleStepMultiplier = MB_SAMPLE_STEP_MULTIPLIER;

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

    // SSAO

    // Motion blur

    if (settings::enableMotionBlur)
    {
        if (SCALE_MB_BUFFERS)
        {
            // Compute tile max
            xGroups = (int)floorf(((float)m_width / (float)MB_SAMPLE_STEP_MULTIPLIER - 1) / 16 + 1);
            yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);
            encoder->setUniform(u_mbSampleStepMultiplier, &sampleStepMultiplier);
            encoder->setTexture(0, s_velocity, gMBVelocity.handle);
            encoder->setImage(1, gMBTileMaxX.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, mbTileMaxXProgram, xGroups, yGroups);

            yGroups = (int)floorf(((float)m_height / (float)MB_SAMPLE_STEP_MULTIPLIER - 1) / 16 + 1);
            encoder->setUniform(u_mbSampleStepMultiplier, &sampleStepMultiplier);
            encoder->setTexture(0, s_mbTileMaxX, gMBTileMaxX.handle);
            encoder->setImage(1, gMBTileMax.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, mbTileMaxYProgram, xGroups, yGroups);
        }

        // JFA iterations
        for (int i = 0; i < MB_JFA_PASS_COUNT; i++)
        {
            float mbJFAData[8] = {
                float(i),
                float(lastIterationIndex),
                MB_PERPEN_ERROR_THRESHOLD,
                sampleStepMultiplier,
                temp_intensity,
                MB_STEP_EXPONENT_MODIFIER,
                maxDilationRadius,
                float(MB_BACKTRACKING_SAMPLE_COUNT)};
            encoder->setUniform(u_mbJFAData, &mbJFAData, 2);
            if (SCALE_MB_BUFFERS)
            {
                encoder->setTexture(0, s_mbTileMax, gMBTileMax.handle);
                encoder->setTexture(1, s_mbBuffer, i % 2 == 0 ? gMBBufferB.handle : gMBBufferA.handle);
                encoder->setImage(2, i % 2 == 0 ? gMBBufferA.handle : gMBBufferB.handle, 0, bgfx::Access::Write);
                encoder->dispatch(VIEW_POSTPROCESS, mbJFAProgram, xGroups, yGroups);
            }
            else
            {
                encoder->setTexture(0, s_depth, gbufDepth.handle);
                encoder->setTexture(1, s_velocity, gMBVelocity.handle);
                encoder->setTexture(2, s_mbBuffer, i % 2 == 0 ? gMBBufferB.handle : gMBBufferA.handle);
                encoder->setImage(3, i % 2 == 0 ? gMBBufferA.handle : gMBBufferB.handle, 0, bgfx::Access::Write);
                encoder->dispatch(VIEW_POSTPROCESS, mbJFABacktrackingProgram, xGroups, yGroups);
            }
        }

        if (SCALE_MB_BUFFERS)
        {
            // Compute neighbor max
            encoder->setTexture(0, s_mbTileMax, gMBTileMax.handle);
            encoder->setTexture(1, s_mbBuffer, lastIterationIndex % 2 == 0 ? gMBBufferA.handle : gMBBufferB.handle);
            encoder->setImage(2, gMBNeighborMax.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, mbNeighborMaxProgram, xGroups, yGroups);
        }

        // Compute blur
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        float mbBlurData[8] = {
            samples,
            temp_intensity,
            centerFade,
            float(mbIndex),
            float(lastIterationIndex),
            sampleStepMultiplier,
            MB_STEP_EXPONENT_MODIFIER,
            maxDilationRadius};

        if (!freezeTemporalEffects)
        {
            mbIndex = (mbIndex + 1) % 8;
        }

        encoder->setUniform(u_mbBlurData, &mbBlurData, 2);

        if (SCALE_MB_BUFFERS)
        {
            encoder->setTexture(0, s_color, gOutputColor.handle);
            encoder->setTexture(1, s_velocity, gMBVelocity.handle);
            encoder->setTexture(2, s_mbNeighborMax, gMBNeighborMax.handle);
            encoder->setImage(3, gMBOutputColor.handle, 0, bgfx::Access::Write);
            encoder->setTexture(4, s_mbTileMax, gMBTileMax.handle);
            encoder->setTexture(5, s_prevColor, gMBPreviousOutputColor.handle);
            encoder->setTexture(6, s_prevVelocity, gMBPreviousVelocity.handle);
            encoder->dispatch(VIEW_POSTPROCESS, mbBlurProgram, xGroups, yGroups);
        }
        else
        {
            encoder->setTexture(0, s_color, gOutputColor.handle);
            encoder->setTexture(1, s_depth, gbufDepth.handle);
            encoder->setTexture(2, s_velocity, gMBVelocity.handle);
            encoder->setTexture(3, s_mbBuffer, lastIterationIndex % 2 == 0 ? gMBBufferA.handle : gMBBufferB.handle);
            encoder->setImage(4, gMBOutputColor.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, mbBlurSimpleProgram, xGroups, yGroups);
        }

        if (SCALE_MB_BUFFERS && !freezeTemporalEffects)
        {
            // Cache
            encoder->setTexture(0, s_velocity, gMBVelocity.handle);
            encoder->setTexture(1, s_color, gOutputColor.handle);
            encoder->setImage(2, gMBPreviousVelocity.handle, 0, bgfx::Access::Write);
            encoder->setImage(3, gMBPreviousOutputColor.handle, 0, bgfx::Access::Write);
            encoder->dispatch(VIEW_POSTPROCESS, mbCacheProgram, xGroups, yGroups);
        }

        // Copy to output
        encoder->setImage(0, gMBOutputColor.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, blitProgram, xGroups, yGroups);
    }

    if (settings::enableTAA)
    {
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
            encoder->setImage(1, gbufDepth.handle, 0, bgfx::Access::Read);
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

    // Bloom
    if(settings::enableBloom)
    {
        float bloomThresholdField[4] = {bloomThreshold, bloomThreshold - bloomKnee, 2 * bloomKnee, 0.25f * bloomKnee};

        float mipSizeX = (float)m_width / 2;
        float mipSizeY = (float)m_height / 2;

        for(uint8_t i=0;i<gOutputColor.mipCount-1;++i) {
            encoder->setUniform(u_bloomThreshold, bloomThresholdField);
            encoder->setTexture(0, s_tex, gOutputColor.handle, 0, 1, i, 1);

            xGroups = (int)floorf((mipSizeX - 1) / 8 + 1);
            yGroups = (int)floorf((mipSizeY - 1) / 8 + 1);

            float bloomTexelSize[4] = {1.0f / mipSizeX, 1.0f / mipSizeY, static_cast<float>(i), 0.0f};
            encoder->setUniform(u_bloomTexelSize, bloomTexelSize);

            encoder->setImage(1, gOutputColor.handle, i+1, bgfx::Access::Write);

            encoder->dispatch(VIEW_POSTPROCESS, bloomDownscaleProgram, xGroups, yGroups);

            mipSizeX /= 2;
            mipSizeY /= 2;
        }

        float bloomIntensityField[4] = {bloomIntensity, bloomDirtIntensity, 0.0f, 0.0f};

        for(uint8_t i=gOutputColor.mipCount-1;i>=1;--i) {
            encoder->setUniform(u_bloomIntensity, bloomIntensityField);
            encoder->setTexture(0, s_tex, gOutputColor.handle, 0, 1, i, 1);
            encoder->setTexture(2, s_bloomDirt, bloomDirtMask.handle);

            mipSizeX = std::max(1.0f, floorf(float(m_width) / (1 << (i-1))));
            mipSizeY = std::max(1.0f, floorf(float(m_height) / (1 << (i-1))));
            
            xGroups = (int)floorf((mipSizeX - 1) / 8 + 1);
            yGroups = (int)floorf((mipSizeY - 1) / 8 + 1);

            float bloomTexelSize[4] = {1.0f / mipSizeX, 1.0f / mipSizeY, static_cast<float>(i), 0.0f};
            encoder->setUniform(u_bloomTexelSize, bloomTexelSize);

            encoder->setImage(1, gOutputColor.handle, i-1, bgfx::Access::ReadWrite);

            encoder->dispatch(VIEW_POSTPROCESS, bloomUpscaleProgram, xGroups, yGroups);
        }
    }

    // Blit and tonemap
    bgfx::setViewFrameBuffer(VIEW_BLIT, BGFX_INVALID_HANDLE);
    bgfx::setViewName(VIEW_BLIT, "Field - Blit");
    bgfx::setViewRect(VIEW_BLIT, 0, 0, uint16_t(m_width), uint16_t(m_height));

    encoder->setTexture(0, s_tex, gOutputColor.handle);
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
    }

    if (bgfx::isValid(u_baseColor))
        bgfx::destroy(u_baseColor);
    if (bgfx::isValid(u_emissionColor))
        bgfx::destroy(u_emissionColor);
    if (bgfx::isValid(u_info))
        bgfx::destroy(u_info);
    if (bgfx::isValid(u_normalMatrix))
        bgfx::destroy(u_normalMatrix);
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

    if (bgfx::isValid(u_mbSampleStepMultiplier))
        bgfx::destroy(u_mbSampleStepMultiplier);
    if (bgfx::isValid(u_mbVelocityData))
        bgfx::destroy(u_mbVelocityData);
    if (bgfx::isValid(u_mbJFAData))
        bgfx::destroy(u_mbJFAData);
    if (bgfx::isValid(u_mbBlurData))
        bgfx::destroy(u_mbBlurData);

    if (bgfx::isValid(u_bloomThreshold))
        bgfx::destroy(u_bloomThreshold);
    if (bgfx::isValid(u_bloomTexelSize))
        bgfx::destroy(u_bloomTexelSize);
    if (bgfx::isValid(u_bloomIntensity))
        bgfx::destroy(u_bloomIntensity);

    if (bgfx::isValid(programPBR))
        bgfx::destroy(programPBR);
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
    if (bgfx::isValid(taaResolveProgram))
        bgfx::destroy(taaResolveProgram);
    if (bgfx::isValid(mbVelocityProgram))
        bgfx::destroy(mbVelocityProgram);
    if (bgfx::isValid(mbTileMaxXProgram))
        bgfx::destroy(mbTileMaxXProgram);
    if (bgfx::isValid(mbTileMaxYProgram))
        bgfx::destroy(mbTileMaxYProgram);
    if (bgfx::isValid(mbJFAProgram))
        bgfx::destroy(mbJFAProgram);
    if (bgfx::isValid(mbJFABacktrackingProgram))
        bgfx::destroy(mbJFABacktrackingProgram);
    if (bgfx::isValid(mbNeighborMaxProgram))
        bgfx::destroy(mbNeighborMaxProgram);
    if (bgfx::isValid(mbBlurProgram))
        bgfx::destroy(mbBlurProgram);
    if (bgfx::isValid(mbBlurSimpleProgram))
        bgfx::destroy(mbBlurSimpleProgram);
    if (bgfx::isValid(mbCacheProgram))
        bgfx::destroy(mbCacheProgram);
    if(bgfx::isValid(bloomDownscaleProgram))
        bgfx::destroy(bloomDownscaleProgram);
    if(bgfx::isValid(bloomUpscaleProgram))
        bgfx::destroy(bloomUpscaleProgram);

    gAccumTex.destroy();
    gRevealTex.destroy();

    gOitFbo.destroy();
    gOitDepthPostPassFbo.destroy();

    gbufAlbedo.destroy();
    gbufEmission.destroy();
    gbufNormal.destroy();
    gbufVelocity.destroy();
    gFullVelocity.destroy();
    gMBPreviousVelocity.destroy();
    gbufDepth.destroy();

    gBufFbo.destroy();

    gOutputColor.destroy();
    gMBPreviousOutputColor.destroy();

    gTAABuffer0.destroy();
    gTAABuffer1.destroy();

    gMBTileMaxX.destroy();
    gMBTileMax.destroy();
    gMBNeighborMax.destroy();
    gMBBufferA.destroy();
    gMBBufferB.destroy();
    gMBOutputColor.destroy();
    gMBVelocity.destroy();

    bloomDirtMask.destroy();

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
    if (bgfx::isValid(s_prevVelocity))
        bgfx::destroy(s_prevVelocity);
    if (bgfx::isValid(s_prevColor))
        bgfx::destroy(s_prevColor);
    if (bgfx::isValid(s_mbTileMaxX))
        bgfx::destroy(s_mbTileMaxX);
    if (bgfx::isValid(s_mbTileMax))
        bgfx::destroy(s_mbTileMax);
    if (bgfx::isValid(s_mbNeighborMax))
        bgfx::destroy(s_mbNeighborMax);
    if (bgfx::isValid(s_mbBuffer))
        bgfx::destroy(s_mbBuffer);
    if (bgfx::isValid(s_bloomDirt))
        bgfx::destroy(s_bloomDirt);
}