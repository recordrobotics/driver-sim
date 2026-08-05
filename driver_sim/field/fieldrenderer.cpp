#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <SDL3/SDL.h>
#include <imgui/imgui.h>

#include <nlohmann/json.hpp>

#include "fieldrenderer.h"
#include "shaders.h"

#include "../settings/settingsstore.h"

#include <bloom_dirt_mask.png.h>
#include <lut.exr.h>

#include <carpet_base_color.jpg.h>
#include <carpet_bump.jpg.h>
#include <apriltag-36h11.png.h>

#include <ledmask.png.h>

#include "../ui/components.h"

#include <shadow.png.h>

#include <mainmenu.png.h>
#include <settings.png.h>
#include <viewmode.png.h>
#include <restartjava.png.h>

using namespace ui;
using blackboard::gui::load_image;

static const bgfx::EmbeddedShader s_embeddedShaders[] =
    {
        BGFX_EMBEDDED_SHADER(vs_pbr),
        BGFX_EMBEDDED_SHADER(vs_pbr_instanced),
        BGFX_EMBEDDED_SHADER(vs_pbr_led),
        BGFX_EMBEDDED_SHADER(vs_pbr_apriltag),
        BGFX_EMBEDDED_SHADER(fs_pbr),
        BGFX_EMBEDDED_SHADER(fs_pbr_led),
        BGFX_EMBEDDED_SHADER(fs_pbr_textured),
        BGFX_EMBEDDED_SHADER(fs_pbr_apriltag),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit),
        BGFX_EMBEDDED_SHADER(fs_oit_moments),

        BGFX_EMBEDDED_SHADER(vs_pass),
        BGFX_EMBEDDED_SHADER(fs_present),

        BGFX_EMBEDDED_SHADER(cs_blit),
        BGFX_EMBEDDED_SHADER(cs_debug_normals),
        BGFX_EMBEDDED_SHADER(cs_tonemap),
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
        BGFX_EMBEDDED_SHADER(cs_XeGTAO_Denoise),
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

static const std::array<std::string, static_cast<size_t>(CameraView::Count)> CAMERA_VIEW_NAMES = {
    "Field",
    "Robot",
    "Robot Relative",
    "Driver Station"};

static const std::array<std::string, static_cast<size_t>(DebugView::Count)> DEBUG_VIEW_NAMES = {
    "None",
    "Albedo",
    "Normal",
    "Emission",
    "PBR Data",
    "Velocity",
    "Depth",
    "OIT Moments",
    "OIT Total Depth",
    "OIT Accum",
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
    "GTAO Working Edges",
    "GTAO Final AO Term Normals",
    "GTAO Final AO Term Visibility"};

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
using blackboard::gui::string_hex_to_rgba_float_array;

static constexpr uint8_t XE_GTAO_DEPTH_MIP_LEVELS = 5;

static constexpr float DRIVER_STATION_CAMERA_HEIGHT = 64 * INCHES_TO_METERS;

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

    template <>
    struct adl_serializer<bx::Vec3>
    {
        static bx::Vec3 from_json(const json &j)
        {
            return {j.at(0).get<float>(), j.at(1).get<float>(), DRIVER_STATION_CAMERA_HEIGHT};
        }
    };
}

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

void FieldRenderer::initPBROIT(uint16_t width, uint16_t height)
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

    s_apriltags = bgfx::createUniform("s_apriltags", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_apriltags))
    {
        logger->error("Failed to create uniform for apriltag texture.");
        throw std::runtime_error("Failed to create uniform for apriltag texture.");
    }

    s_ledMask = bgfx::createUniform("s_ledMask", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_ledMask))
    {
        logger->error("Failed to create uniform for led mask texture.");
        throw std::runtime_error("Failed to create uniform for led mask texture.");
    }

    s_ledColors = bgfx::createUniform("s_ledColors", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_ledColors))
    {
        logger->error("Failed to create uniform for led colors texture.");
        throw std::runtime_error("Failed to create uniform for led colors texture.");
    }

    s_momentsTex = bgfx::createUniform("s_momentsTex", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_momentsTex))
    {
        logger->error("Failed to create uniform for moments texture.");
        throw std::runtime_error("Failed to create uniform for moments texture.");
    }

    s_totalDepthTex = bgfx::createUniform("s_totalDepthTex", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_totalDepthTex))
    {
        logger->error("Failed to create uniform for total depth texture.");
        throw std::runtime_error("Failed to create uniform for total depth texture.");
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

    u_gtaoIntensity = bgfx::createUniform("u_gtaoIntensity", bgfx::UniformType::Vec4);

    if (!bgfx::isValid(u_gtaoIntensity))
    {
        logger->error("Failed to create uniform: u_gtaoIntensity");
        throw std::runtime_error("Failed to create uniform: u_gtaoIntensity");
    }

    u_info = bgfx::createUniform("u_info", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4, 2);

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

    u_ledData = bgfx::createUniform("u_ledData", bgfx::UniformType::Vec4);
    if (!bgfx::isValid(u_ledData))
    {
        logger->error("Failed to create uniform: u_ledData");
        throw std::runtime_error("Failed to create uniform: u_ledData");
    }

    u_lightPos = bgfx::createUniform("u_lightPos", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4, LIGHT_COUNT);
    if (!bgfx::isValid(u_lightPos))
    {
        logger->error("Failed to create uniform: u_lightPos");
        throw std::runtime_error("Failed to create uniform: u_lightPos");
    }

    u_lightColor = bgfx::createUniform("u_lightColor", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4, LIGHT_COUNT);
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

    programPBRLed =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_led"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_led"), true);

    if (!bgfx::isValid(programPBRLed))
    {
        logger->error("Failed to create PBR led rendering program.");
        throw std::runtime_error("Failed to create PBR led rendering program.");
    }

    programPBRApriltag =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_apriltag"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_apriltag"), true);

    if (!bgfx::isValid(programPBRApriltag))
    {
        logger->error("Failed to create PBR apriltag rendering program.");
        throw std::runtime_error("Failed to create PBR apriltag rendering program.");
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

    programOitMoments =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_oit_moments"), true);

    if (!bgfx::isValid(programOitMoments))
    {
        logger->error("Failed to create OIT moments program.");
        throw std::runtime_error("Failed to create OIT moments program.");
    }

    programOitMomentsInstanced =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr_instanced"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_oit_moments"), true);

    if (!bgfx::isValid(programOitMomentsInstanced))
    {
        logger->error("Failed to create OIT instanced moments program.");
        throw std::runtime_error("Failed to create OIT instanced moments program.");
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
        gMomentsTex,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA32F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    TEXTURE(
        gTotalDepthTex,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R32F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    FRAMEBUFFER(
        gOitFbo,
        &gAccumTex, &gbufDepth);

    FRAMEBUFFER(
        gOitMomentsFbo,
        &gMomentsTex, &gTotalDepthTex, &gbufDepth);

    TEXTURE(
        gOutputColor,
        width, height,
        1.0f, 1.0f,
        true,
        1,
        bgfx::TextureFormat::RG11B10F,
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

    TEXTURE_EMBEDDED(
        apriltagTexture,
        apriltag_36h11_png,
        bimg::TextureFormat::R8,
        bgfx::TextureFormat::R8,
        0);

    aprilTagMesh.material.texture = "apriltags";
    aprilTagMesh.material.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    aprilTagMesh.material.roughness = 0.7f;
    float apriltag36h11ScaleRatio = 10.0f / 8.0f;
    Mesh::addCube(
        aprilTagMesh,
        -0.01f / 2, 0.0f, 0.0f,
        0.01f, apriltag36h11ScaleRatio, apriltag36h11ScaleRatio,
        true /* apriltag texture is projected for the purposes of pose estimation */);

    TEXTURE_EMBEDDED(
        ledMaskTexture,
        ledmask_png,
        bimg::TextureFormat::R8,
        bgfx::TextureFormat::R8,
        0);
}

void FieldRenderer::initTonemap()
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
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_tonemap"), true);

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

void FieldRenderer::initTAA(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_taaHistory = bgfx::createUniform("s_taaHistory", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_taaHistory))
    {
        logger->error("Failed to create uniform for TAA history texture.");
        throw std::runtime_error("Failed to create uniform for TAA history texture.");
    }

    u_jitter = bgfx::createUniform("u_jitter", bgfx::UniformFreq::Frame, bgfx::UniformType::Vec4);
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
        bgfx::TextureFormat::RG11B10F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gTAABuffer1,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RG11B10F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);
}

void FieldRenderer::initGBuffer(uint16_t width, uint16_t height)
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
        bgfx::TextureFormat::RG11B10F,
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

void FieldRenderer::initMotionBlur(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_mbTileMaxX = bgfx::createUniform("s_tilemax_x", bgfx::UniformType::Sampler);
    s_mbTileMax = bgfx::createUniform("s_tilemax", bgfx::UniformType::Sampler);
    s_mbNeighborMax = bgfx::createUniform("s_neighbormax", bgfx::UniformType::Sampler);
    s_mbTileVariance = bgfx::createUniform("s_tilevariance", bgfx::UniformType::Sampler);

    u_previousView = bgfx::createUniform("u_previousView", bgfx::UniformFreq::Frame, bgfx::UniformType::Mat4);
    if (!bgfx::isValid(u_previousView))
    {
        logger->error("Failed to create uniform: u_previousView");
        throw std::runtime_error("Failed to create uniform: u_previousView");
    }

    u_previousProj = bgfx::createUniform("u_previousProj", bgfx::UniformFreq::Frame, bgfx::UniformType::Mat4);
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
        bgfx::TextureFormat::R16F,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    TEXTURE(
        gMBOutputColor,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RG11B10F,
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

void FieldRenderer::initBloom(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    s_bloomInput = bgfx::createUniform("s_bloomInput", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_bloomInput))
    {
        logger->error("Failed to create uniform: s_bloomInput");
        throw std::runtime_error("Failed to create uniform: s_bloomInput");
    }

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
#define XE_HILBERT_LEVEL 6U
#define XE_HILBERT_WIDTH ((1U << XE_HILBERT_LEVEL))
#define XE_HILBERT_AREA (XE_HILBERT_WIDTH * XE_HILBERT_WIDTH)
inline uint32_t HilbertIndex(uint32_t posX, uint32_t posY)
{
    uint32_t index = 0U;
    for (uint32_t curLevel = XE_HILBERT_WIDTH / 2U; curLevel > 0U; curLevel /= 2U)
    {
        uint32_t regionX = (posX & curLevel) > 0U;
        uint32_t regionY = (posY & curLevel) > 0U;
        index += curLevel * curLevel * ((3U * regionX) ^ regionY);
        if (regionY == 0U)
        {
            if (regionX == 1U)
            {
                posX = uint32_t((XE_HILBERT_WIDTH - 1U)) - posX;
                posY = uint32_t((XE_HILBERT_WIDTH - 1U)) - posY;
            }

            uint32_t temp = posX;
            posX = posY;
            posY = temp;
        }
    }
    return index;
}

void FieldRenderer::initGTAO(uint16_t width, uint16_t height)
{
    const auto type = bgfx::getRendererType();

    u_XeGTAOData = bgfx::createUniform("u_XeGTAOData", bgfx::UniformType::Vec4, 3);
    if (!bgfx::isValid(u_XeGTAOData))
    {
        logger->error("Failed to create uniform: u_XeGTAOData");
        throw std::runtime_error("Failed to create uniform: u_XeGTAOData");
    }

    s_workingAOTerm = bgfx::createUniform("s_workingAOTerm", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_workingAOTerm))
    {
        logger->error("Failed to create uniform: s_workingAOTerm");
        throw std::runtime_error("Failed to create uniform: s_workingAOTerm");
    }

    s_workingEdges = bgfx::createUniform("s_workingEdges", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_workingEdges))
    {
        logger->error("Failed to create uniform: s_workingEdges");
        throw std::runtime_error("Failed to create uniform: s_workingEdges");
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

    XeGTAO_DenoiseProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_XeGTAO_Denoise"), true);
    if (!bgfx::isValid(XeGTAO_DenoiseProgram))
    {
        logger->error("Failed to create XeGTAO Denoise program.");
        throw std::runtime_error("Failed to create XeGTAO Denoise program.");
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
        uint16_t *data = new uint16_t[64 * 64];
        for (int x = 0; x < 64; x++)
        {
            for (int y = 0; y < 64; y++)
            {
                uint32_t r2index = HilbertIndex(x, y);
                assert(r2index < 65536);
                data[x + 64 * y] = (uint16_t)r2index;
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
            bgfx::copy(data, 64 * 64 * sizeof(uint16_t)));

        delete[] data;
    }

    TEXTURE(
        gGTAOFinalAOTerm,
        width, height,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::R32U,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);
}

frc::Pose3d FieldRenderer::transformPose3dToLocalCoordinates(const frc::Pose3d &pose) const
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

Transform::Transform()
{
    std::memset(this, 0, sizeof(Transform));
}

Transform::Transform(float *modelMatrix, float *parentMatrix, bx::Vec3 position, bx::Quaternion rotation)
{
    float matrix[16];
    float rotationMatrix[16];
    bx::mtxFromQuaternion(rotationMatrix, rotation);
    float translationMatrix[16];
    bx::mtxTranslate(translationMatrix, position.x, position.y, position.z);
    if (modelMatrix == nullptr && parentMatrix == nullptr)
    {
        bx::mtxMul(matrix, rotationMatrix, translationMatrix);
    }
    else
    {
        float relativeMatrix[16];
        bx::mtxMul(relativeMatrix, rotationMatrix, translationMatrix);
        if (modelMatrix == nullptr)
        {
            bx::mtxMul(matrix, relativeMatrix, parentMatrix);
        }
        else if (parentMatrix == nullptr)
        {
            bx::mtxMul(matrix, modelMatrix, relativeMatrix);
        }
        else
        {
            float finalMatrix[16];
            bx::mtxMul(finalMatrix, modelMatrix, relativeMatrix);
            bx::mtxMul(matrix, finalMatrix, parentMatrix);
        }
    }

    float transpose[16];
    bx::mtxTranspose(transpose, matrix);
    std::memcpy(this, transpose, sizeof(Transform));
}

void Transform::toMatrix(float *outMatrix) const
{
    float transpose[16];
    std::memcpy(transpose, this, sizeof(Transform));
    transpose[12] = 0.0f;
    transpose[13] = 0.0f;
    transpose[14] = 0.0f;
    transpose[15] = 1.0f;
    bx::mtxTranspose(outMatrix, transpose);
}

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

void DynamicObjectData::update(float *modelMatrix, float *parentMatrix, float deltaTime, bool freezeTemporalEffects)
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

    instanceData.transform = {
        modelMatrix,
        parentMatrix,
        lastPosition,
        lastRotation};

    if (lastDataUpdate == -1)
    {
        instanceData.previousTransform = instanceData.transform;
    }
}

RobotData::RobotData(RobotModel *model)
    : model(model), components(model->components.size()), bumperBaseColor(model->bumperModelColor)
{
    uint16_t ledCount = static_cast<uint16_t>(floorf(model->ledCount));
    ledColorData = std::make_unique<uint8_t[]>(ledCount * 4);
    TEXTURE(
        ledColorTexture,
        ledCount, 1,
        1.0f, 1.0f,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
}

void RobotData::update(int currentDataUpdateIndex, float deltaTime, bool freezeTemporalEffects, std::function<frc::Pose3d(const frc::Pose3d &)> transformPose3dToLocalCoordinates)
{
    if (poseSub.Exists() && (alwaysEnabled || (enabledSub.Exists() && enabledSub.GetAtomic().value)))
    {
        auto robotPose = poseSub.GetAtomic();
        frc::Pose3d localPose = transformPose3dToLocalCoordinates(robotPose.value);
        dynamicData.position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
        dynamicData.rotation = rotation3dToQuaternion(localPose.Rotation());
        dynamicData.update(model->modelMatrix.data(), deltaTime, freezeTemporalEffects);
        dynamicData.lastDataUpdate = currentDataUpdateIndex;

        // Update component poses
        auto componentPoses = componentPosesSub.GetAtomic();

        float robotOriginMtx[16];
        float rotationMatrix[16];
        bx::mtxFromQuaternion(rotationMatrix, dynamicData.lastRotation);
        float translationMatrix[16];
        bx::mtxTranslate(translationMatrix, dynamicData.lastPosition.x, dynamicData.lastPosition.y, dynamicData.lastPosition.z);
        bx::mtxMul(robotOriginMtx, rotationMatrix, translationMatrix);

        for (size_t i = 0; i < components.size(); ++i)
        {
            if (componentPosesSub.Exists() && i < componentPoses.value.size())
            {
                // invert rotation to match advscope
                components[i].position = {static_cast<float>(componentPoses.value[i].X().value()), static_cast<float>(componentPoses.value[i].Y().value()), static_cast<float>(componentPoses.value[i].Z().value())};
                components[i].rotation = rotation3dToQuaternionInverse(componentPoses.value[i].Rotation());
                components[i].update(model->components[i].modelMatrix.data(), robotOriginMtx, deltaTime, freezeTemporalEffects);
            }
            else
            {
                components[i].position = {0.0f, 0.0f, 0.0f};
                components[i].rotation = rotation3dToQuaternion(frc::Rotation3d{});
                components[i].update(model->modelMatrix.data(), robotOriginMtx, deltaTime, freezeTemporalEffects);
            }

            components[i].lastDataUpdate = currentDataUpdateIndex;
        }

        // Update RSL state
        if (rslStateSub.Exists())
        {
            rslEmissionStrength = rslStateSub.GetAtomic().value ? model->rslOnEmissionStrength : 0.0f;
        }

        // Update bumper color
        if (allianceStationSub.Exists())
        {
            int allianceStation = allianceStationSub.GetAtomic().value;
            if ((allianceStation == 1 || allianceStation == 2 || allianceStation == 3) && model->bumperRedColor.has_value()) // Red
            {
                bumperBaseColor = model->bumperRedColor.value();
            }
            else if ((allianceStation == 4 || allianceStation == 5 || allianceStation == 6) && model->bumperBlueColor.has_value()) // Blue
            {
                bumperBaseColor = model->bumperBlueColor.value();
            }
            else
            {
                bumperBaseColor = model->bumperModelColor;
            }
        }

        // Update LED colors
        if (ledColorsSub.Exists())
        {
            std::vector<std::string> ledColors = ledColorsSub.GetAtomic().value;
            for (uint16_t i = 0; i < ledColorTexture.width; ++i)
            {
                if (i < ledColors.size())
                {
                    std::string colorStr = ledColors[i];
                    if (colorStr.length() == 7 && colorStr[0] == '#')
                    {
                        uint8_t r = std::stoi(colorStr.substr(1, 2), nullptr, 16);
                        uint8_t g = std::stoi(colorStr.substr(3, 2), nullptr, 16);
                        uint8_t b = std::stoi(colorStr.substr(5, 2), nullptr, 16);
                        ledColorData[i * 4 + 0] = r;
                        ledColorData[i * 4 + 1] = g;
                        ledColorData[i * 4 + 2] = b;
                        ledColorData[i * 4 + 3] = 255;
                    }
                }
                else
                {
                    ledColorData[i * 4 + 0] = 0;
                    ledColorData[i * 4 + 1] = 0;
                    ledColorData[i * 4 + 2] = 0;
                    ledColorData[i * 4 + 3] = 255;
                }
            }
        }
        else
        {
            for (uint16_t i = 0; i < ledColorTexture.width; ++i)
            {
                ledColorData[i * 4 + 0] = 0;
                ledColorData[i * 4 + 1] = 0;
                ledColorData[i * 4 + 2] = 0;
                ledColorData[i * 4 + 3] = 255;
            }
        }
    }
    else
    {
        dynamicData.lastDataUpdate = -1;
        for (auto &component : components)
        {
            component.lastDataUpdate = -1;
        }
    }
}

void GamePieceData::update(int currentDataUpdateIndex, float deltaTime, bool freezeTemporalEffects, std::function<frc::Pose3d(const frc::Pose3d &)> transformPose3dToLocalCoordinates)
{
    bool hasObjects = poseObjectsSub.Exists();
    if (hasObjects || posesSub.Exists())
    {
        auto updateInstance = [&](DynamicObjectData &instance, const frc::Pose3d &pose)
        {
            auto localPose = transformPose3dToLocalCoordinates(pose);
            instance.position = {static_cast<float>(localPose.X().value()), static_cast<float>(localPose.Y().value()), static_cast<float>(localPose.Z().value())};
            instance.rotation = rotation3dToQuaternion(localPose.Rotation());
            instance.update(modelMatrix.data(), deltaTime, freezeTemporalEffects);
            instance.lastDataUpdate = currentDataUpdateIndex;
        };

        if (hasObjects)
        {
            auto gamePiecePoseObjects = poseObjectsSub.GetAtomic();
            for (size_t i = 0; i < gamePiecePoseObjects.value.size(); ++i)
            {
                updateInstance(instances[gamePiecePoseObjects.value[i].identity], gamePiecePoseObjects.value[i].pose);
            }
        }
        else
        {
            auto gamePiecePoses = posesSub.GetAtomic();
            for (size_t i = 0; i < gamePiecePoses.value.size(); ++i)
            {
                updateInstance(instances[i], gamePiecePoses.value[i]);
            }
        }
        std::erase_if(instances, [currentDataUpdateIndex](const std::pair<int, DynamicObjectData> &pair)
                      { return pair.second.lastDataUpdate != currentDataUpdateIndex; });
    }
    else
    {
        instances.clear();
    }
}

void FieldRenderer::setRestartSimulationCallback(std::function<void()> callback)
{
    restartSimulationCallback = callback;
}

void FieldRenderer::updateOrbitCameraFromInput()
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

/**
 * tags is a map of mesh-name:tag
 * meshes of the same material but different tags will stay separated and won't be merged.
 * this is useful for dynamically removing meshes based on tags.
 * additionally, meshes with same tag but different materials will also stay separated, since they can't be merged anyway.
 */
static void loadAndCacheMeshes(std::vector<Mesh> &meshes, std::string directory, std::string name, const std::unordered_map<std::string, std::string> &tags)
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
    fastgltf::Parser parser(fastgltf::Extensions::KHR_materials_emissive_strength);
    Mesh::fromGltfModel(meshes, parser.loadGltfBinary(fastgltf::GltfDataBuffer::FromPath(glbPath.string()).get(), directory, fastgltf::Options::LoadGLBBuffers | fastgltf::Options::DontRequireValidAssetMember).get(), tags);

    if (settings::cacheModels)
    {
        logger->info("Caching {0} meshes to disk.", directory + name);
        Mesh::toSerialized(meshes, cachePath);
    }
}

void FieldRenderer::loadFieldModel()
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

        performRotationStack(fieldModelMatrix[0], j["rotations"].get<std::vector<ModelRotationConfig>>());
        std::memcpy(fieldModelMatrix[1], fieldModelMatrix[0], sizeof(float) * 16);

        coordinateSystem = coordinateSystemFromString(j["coordinateSystem"].get<std::string>());
        fieldWidthMeters = j["widthInches"].get<float>() * INCHES_TO_METERS;
        fieldHeightMeters = j["heightInches"].get<float>() * INCHES_TO_METERS;

        std::unordered_map<std::string, std::string> tags;

        if (j.contains("gamePieces"))
        {
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
        }

        if (j.contains("aprilTags"))
        {
            for (const auto &aprilTag : j["aprilTags"])
            {
                // Format as "FAMILY-SIZEin" where "FAMILY" is "36h11" or "16h5" and "SIZE" is the length of the black section
                std::string variant = aprilTag["variant"];
                std::string family = variant.substr(0, variant.find('-'));
                std::string size = variant.substr(variant.find('-') + 1, variant.find("in") - variant.find('-') - 1);

                if (family != "36h11")
                {
                    logger->warn("AprilTag family {} is not supported. Only 36h11 is supported.", family);
                    continue;
                }

                float tagSizeInches = std::stof(size);
                float tagSizeMeters = tagSizeInches * INCHES_TO_METERS;

                float scaleMtx[16];
                bx::mtxScale(scaleMtx, 1.0f, tagSizeMeters, tagSizeMeters);
                float rotationMtx[16];
                performRotationStack(rotationMtx, aprilTag["rotations"].get<std::vector<ModelRotationConfig>>());
                const auto &position = aprilTag["position"].get<std::vector<float>>();
                float translationMtx[16];
                bx::mtxTranslate(translationMtx, position[0], position[1], position[2]);
                std::array<float, 16> tmp;
                bx::mtxMul(tmp.data(), scaleMtx, rotationMtx);
                std::array<float, 16> aprilTagModelMatrix;
                bx::mtxMul(aprilTagModelMatrix.data(), tmp.data(), translationMtx);

                uint32_t id = aprilTag["id"].get<uint32_t>();

                AprilTagInstanceData data = {
                    .modelMatrix = aprilTagModelMatrix,
                };

                data.id() = static_cast<float>(id) + 0.5f;

                aprilTags.push_back(data);
            }
        }

        if (j.contains("driverStations"))
        {
            driverStationCameraPositions = j["driverStations"].get<std::vector<bx::Vec3>>();
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

void FieldRenderer::loadRobotModel()
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

        std::string robotName = j["name"].get<std::string>();

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

        RobotModel &robotModel = robotModels.emplace_back(robotName, std::to_array(robotModelMatrix), components);

        std::vector<std::string> rslNodeNames = j.value("rsl", std::vector<std::string>());

        std::vector<std::string> bumperNodeNames;
        if (j.contains("bumper"))
        {
            const auto &bumperObject = j["bumper"];
            if (bumperObject.contains("name"))
            {
                bumperNodeNames = bumperObject["name"].get<std::vector<std::string>>();
            }
            else
            {
                logger->warn("Bumper object exists but does not contain a name field.");
            }

            if (bumperObject.contains("redColor"))
            {
                std::string redColorHex = bumperObject["redColor"].get<std::string>();
                if (redColorHex.size() == 9 && redColorHex[0] == '#')
                {
                    robotModel.bumperRedColor = string_hex_to_rgba_float_array(redColorHex);
                }
                else
                {
                    logger->warn("Bumper redColor field is not in the correct format: {}", redColorHex);
                }
            }

            if (bumperObject.contains("blueColor"))
            {
                std::string blueColorHex = bumperObject["blueColor"].get<std::string>();
                if (blueColorHex.size() == 9 && blueColorHex[0] == '#')
                {
                    robotModel.bumperBlueColor = string_hex_to_rgba_float_array(blueColorHex);
                }
                else
                {
                    logger->warn("Bumper blueColor field is not in the correct format: {}", blueColorHex);
                }
            }
        }

        std::vector<std::string> ledNodeNames;
        if (j.contains("led"))
        {
            const auto &ledObject = j["led"];
            if (ledObject.contains("name"))
            {
                ledNodeNames = ledObject["name"].get<std::vector<std::string>>();
            }
            else
            {
                logger->warn("LED object exists but does not contain a name field.");
            }

            if (ledObject.contains("count"))
            {
                robotModel.ledCount = ledObject["count"].get<float>();
            }
            else
            {
                logger->warn("LED object exists but does not contain a count field.");
            }

            if (ledObject.contains("aspectRatio"))
            {
                robotModel.ledAspectRatio = ledObject["aspectRatio"].get<float>();
            }
            else
            {
                logger->warn("LED object exists but does not contain an aspectRatio field.");
            }
        }

        std::unordered_map<std::string, std::string> tags;

        for (const auto &rslNodeName : rslNodeNames)
        {
            tags[rslNodeName] = "rsl";
        }

        for (const auto &bumperNodeName : bumperNodeNames)
        {
            tags[bumperNodeName] = "bumper";
        }

        for (const auto &ledNodeName : ledNodeNames)
        {
            tags[ledNodeName] = "led";
        }

        loadAndCacheMeshes(robotModel.meshes, robotDirectory, "model", tags);
        for (auto &mesh : robotModel.meshes)
        {
            mesh.material.writesObjectMotionVectors = true;
        }
        for (size_t i = 0; i < components.size(); i++)
        {
            loadAndCacheMeshes(robotModel.components[i].meshes, robotDirectory, "model_" + std::to_string(i), {});
            for (auto &mesh : robotModel.components[i].meshes)
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

void FieldRenderer::startLoadFieldModel()
{
    if (startedLoadingFieldModel)
    {
        return;
    }

    startedLoadingFieldModel = true;

    logger->info("Starting loading field model in background thread.");
    fieldModelLoadingFuture = std::async(std::launch::async, [this]()
                                         { loadFieldModel(); });
}

void FieldRenderer::startLoadRobotModel()
{
    if (startedLoadingRobotModel)
    {
        return;
    }

    startedLoadingRobotModel = true;

    logger->info("Starting loading robot model in background thread.");
    robotModelLoadingFuture = std::async(std::launch::async, [this]()
                                         { loadRobotModel(); });
}

FieldRenderer::FieldRenderer(const blackboard::app::Window &window)
{
    skyColor = SRGBToLinear({0.54f, 0.54f, 0.6f, 1.0f});
    lightColor = {
        SRGBToLinear({1.0f, 0.25f, 0.25f, 432.0f}),
        SRGBToLinear({1.0f, 0.85f, 0.85f, 332.0f}),
        SRGBToLinear({1.0f, 1.0f, 1.0f, 432.0f}),
        SRGBToLinear({1.0f, 1.0f, 1.0f, 332.0f}),
        SRGBToLinear({0.25f, 0.45f, 1.0f, 432.0f}),
        SRGBToLinear({0.65f, 0.85f, 1.0f, 332.0f})};

    MeshVertex::init();
    UVVertex::init();

    font = blackboard::gui::get_font("Inter_Regular_otf");

    load_image((void *)shadow_png_bytes, sizeof(shadow_png_bytes), shadowTexture, BGFX_SAMPLER_UVW_CLAMP);

    load_image((void *)mainmenu_png_bytes, sizeof(mainmenu_png_bytes), mainMenuTexture);
    load_image((void *)settings_png_bytes, sizeof(settings_png_bytes), settingsTexture);
    load_image((void *)viewmode_png_bytes, sizeof(viewmode_png_bytes), viewModeTexture);
    load_image((void *)restartjava_png_bytes, sizeof(restartjava_png_bytes), restartJavaTexture);

    s_texPresent = bgfx::createUniform("s_texPresent", bgfx::UniformType::Sampler);

    if (!bgfx::isValid(s_texPresent))
    {
        logger->error("Failed to create uniform for present texture.");
        throw std::runtime_error("Failed to create uniform for present texture.");
    }

    u_previousViewProj = bgfx::createUniform("u_previousViewProj", bgfx::UniformFreq::Frame, bgfx::UniformType::Mat4);
    if (!bgfx::isValid(u_previousViewProj))
    {
        logger->error("Failed to create uniform: u_previousViewProj");
        throw std::runtime_error("Failed to create uniform: u_previousViewProj");
    }

    const auto type = bgfx::getRendererType();

    blitProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_blit"), true);

    if (!bgfx::isValid(blitProgram))
    {
        logger->error("Failed to create blit program.");
        throw std::runtime_error("Failed to create blit program.");
    }

    presentProgram =
        bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pass"),
                            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_present"), true);

    if (!bgfx::isValid(presentProgram))
    {
        logger->error("Failed to create present program.");
        throw std::runtime_error("Failed to create present program.");
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

void FieldRenderer::startNTClient()
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

constexpr float CAMERA_LINEAR_FAR = 100.0f;

// assumes reverse z, infinite far
void FieldRenderer::updateInfo(float cameraNear, float proj[16])
{
    float info[8] = {
        cameraNear,
        std::log(cameraNear),
        CAMERA_LINEAR_FAR - cameraNear,
        std::log(CAMERA_LINEAR_FAR) - std::log(cameraNear),
        2.0f / proj[0],
        -2.0f / proj[5],
        -1.0f / proj[0],
        1.0f / proj[5]};
    bgfx::setFrameUniform(u_info, info, 2);
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

void FieldRenderer::ensureTextures(uint16_t width, uint16_t height)
{
    gAccumTex.beginFrame();
    gMomentsTex.beginFrame();
    gTotalDepthTex.beginFrame();

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
    gGTAOFinalAOTerm.beginFrame();

    gAccumTex.ensure(width, height);
    gMomentsTex.ensure(width, height);
    gTotalDepthTex.ensure(width, height);

    gOitFbo.ensure(width, height);
    gOitMomentsFbo.ensure(width, height);

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
    gGTAOFinalAOTerm.ensure(width, height);
}

void FieldRenderer::setupMesh(bgfx::Encoder *encoder, const Mesh &mesh, bool isTransparentPrepass)
{
    float pbrData[4] = {
        (mesh.material.writesObjectMotionVectors && settings::writeObjectMotionVectors) ? 1.0f : 0.0f,
        mesh.material.metallic,
        mesh.material.roughness,
        0.0f};

    if (mesh.material.type == MaterialType::Transparent)
    {
        encoder->setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_GREATER |
            // Additive blending
            BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                  BGFX_STATE_BLEND_ONE));
        if (!isTransparentPrepass)
        {
            // Use moments prepass for OIT
            encoder->setTexture(0, s_momentsTex, gMomentsTex.handle);
            encoder->setTexture(1, s_totalDepthTex, gTotalDepthTex.handle);
        }
    }
    else
    {
        encoder->setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_GREATER |
            BGFX_STATE_WRITE_Z |
            BGFX_STATE_CULL_CW);
    }

    encoder->setVertexBuffer(0, mesh.vertexBuffer);
    encoder->setIndexBuffer(mesh.indexBuffer);

    encoder->setUniform(u_baseColor, SRGBToLinear(mesh.material.baseColor).data());
    encoder->setUniform(u_emissionColor, SRGBToLinear(mesh.material.emissionColor).data());
    encoder->setUniform(u_pbrData, pbrData);

    if (mesh.material.texture == "carpet")
    {
        encoder->setTexture(0, s_baseColor, carpetBaseColor.handle);
        encoder->setTexture(1, s_bump, carpetBump.handle);
    }
    else if (mesh.material.texture == "apriltags")
    {
        encoder->setTexture(0, s_apriltags, apriltagTexture.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
    }
    else if (mesh.material.texture == "led")
    {
        encoder->setTexture(0, s_ledMask, ledMaskTexture.handle, BGFX_SAMPLER_UVW_CLAMP);
    }
}

void FieldRenderer::drawAprilTags(bgfx::Encoder *encoder)
{
    if (aprilTags.empty())
    {
        return;
    }

    // figure out how big of a buffer is available
    uint32_t instanceCount = bgfx::getAvailInstanceDataBuffer(aprilTags.size(), sizeof(AprilTagInstanceData));

    bgfx::InstanceDataBuffer idb;
    bgfx::allocInstanceDataBuffer(&idb, instanceCount, sizeof(AprilTagInstanceData));

    std::memcpy(idb.data, aprilTags.data(), instanceCount * sizeof(AprilTagInstanceData));

    setupMesh(encoder, aprilTagMesh);
    encoder->setInstanceDataBuffer(&idb);
    encoder->submit(VIEW_GBUFFER, programPBRApriltag);
}

void FieldRenderer::addRobot(std::string_view poseTopic, std::string_view componentPosesTopic, std::string_view rslStateTopic, std::string_view allianceStationTopic, std::string_view ledColorsTopic, std::string_view enabledTopic)
{
    RobotData &robot = robots[&robotModels[0]].emplace_back(&robotModels[0]);

    robot.poseTopic = ntInst.GetStructTopic<frc::Pose3d>(poseTopic);
    robot.poseSub = robot.poseTopic.Subscribe(frc::Pose3d{}, {.periodic = settings::ntPeriodic});
    robot.componentPosesTopic = ntInst.GetStructArrayTopic<frc::Pose3d>(componentPosesTopic);
    robot.componentPosesSub = robot.componentPosesTopic.Subscribe({}, {.periodic = settings::ntPeriodic});

    robot.rslStateTopic = ntInst.GetBooleanTopic(rslStateTopic);
    robot.rslStateSub = robot.rslStateTopic.Subscribe(false, {.periodic = settings::ntPeriodic});

    robot.allianceStationTopic = ntInst.GetIntegerTopic(allianceStationTopic);
    robot.allianceStationSub = robot.allianceStationTopic.Subscribe(1, {.periodic = settings::ntPeriodic});

    robot.ledColorsTopic = ntInst.GetStringArrayTopic(ledColorsTopic);
    robot.ledColorsSub = robot.ledColorsTopic.Subscribe({}, {.periodic = settings::ntPeriodic});

    if (!enabledTopic.empty())
    {
        robot.enabledTopic = ntInst.GetBooleanTopic(enabledTopic);
        robot.enabledSub = robot.enabledTopic.Subscribe(false, {.periodic = settings::ntPeriodic});
    }
    else
    {
        robot.alwaysEnabled = true;
    }
}

void FieldRenderer::drawDebugMenu()
{
    if (!ImGui::Begin("Debug Menu"))
    {
        ImGui::End();
        return;
    }

    ImGui::Checkbox("Freeze Temporal Effects", &freezeTemporalEffects);

    ImGui::Separator();

    if (settings::enableMotionBlur)
    {
        ImGui::Text("Motion Blur Settings");
        int tileSize = MB_TILE_SIZE;
        ImGui::SliderInt("Tile Size", &tileSize, 8, 128);
        MB_TILE_SIZE = static_cast<uint8_t>(tileSize);
        int sampleCount = MB_SAMPLE_COUNT;
        ImGui::SliderInt("Sample Count", &sampleCount, 4, 64);
        MB_SAMPLE_COUNT = static_cast<uint8_t>(sampleCount);

        ImGui::Separator();
    }

    if (settings::enableBloom)
    {
        ImGui::Text("Bloom Settings");
        ImGui::SliderFloat("Threshold", &bloomThreshold, 0.0f, 8.0f);
        ImGui::Separator();
    }

    ImGui::Text("Tonemapping Settings");
    ImGui::SliderFloat("Exposure", &tonemappingExposure, -15.0f, 10.0f);
    ImGui::Separator();

    if (settings::enableGTAO)
    {
        ImGui::Text("GTAO Settings");

        ImGui::SliderFloat("Effect Radius", &gtaoEffectRadius, 0.01f, 3.0f);
        ImGui::SliderFloat("Radius Multiplier", &gtaoRadiusMultiplier, 0.25f, 2.0f);
        ImGui::SliderFloat("Intensity", &gtaoIntensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Direct Intensity", &gtaoDirectIntensity, 0.0f, 3.0f);
        ImGui::SliderFloat("Bent Normal Intensity", &gtaoBentNormalIntensity, 0.0f, 3.0f);

        ImGui::Separator();
    }

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

    ImGui::End();
}

void FieldRenderer::drawTopUI()
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;
    ImGuiViewport *viewport = ImGui::GetMainViewport();

    const std::chrono::duration<float> stayDuration = std::chrono::seconds(5);

    ImGuiIO &io = ImGui::GetIO();
    if (!io.WantCaptureMouse)
    {
        if (io.MousePos.x >= viewport->Pos.x &&
            io.MousePos.x < viewport->Pos.x + viewport->Size.x &&
            io.MousePos.y >= viewport->Pos.y &&
            io.MousePos.y < viewport->Pos.y + 300.0f * globalScale &&
            (std::abs(io.MouseDelta.x) > 1.0f || std::abs(io.MouseDelta.y) > 1.0f))
        {
            if (!isTopUISummoned)
            {
                firstTopUISummonTime = std::chrono::steady_clock::now();
                isTopUISummoned = true;
            }

            lastTopUISummonTime = std::chrono::steady_clock::now();
        }
        else if (std::chrono::duration<float>(std::chrono::steady_clock::now() - lastTopUISummonTime).count() >= stayDuration.count())
        {
            isTopUISummoned = false;
        }
    }

    const float inAnimationTime = 0.2f;
    const float outAnimationTime = 0.3f;
    float inAnimationProgress = std::clamp(std::chrono::duration<float>(std::chrono::steady_clock::now() - firstTopUISummonTime).count() / inAnimationTime, 0.0f, 1.0f);
    float outAnimationProgress = std::clamp((std::chrono::duration<float>(std::chrono::steady_clock::now() - lastTopUISummonTime).count() - stayDuration.count()) / outAnimationTime, 0.0f, 1.0f);
    bool isTopUIVisible = inAnimationProgress < 1.0f || outAnimationProgress < 1.0f;

    if (!isTopUIVisible)
    {
        return;
    }

    bool isAnimatingIn = inAnimationProgress < 1.0f;
    float animationProgress = isAnimatingIn ? inAnimationProgress : (1.0f - outAnimationProgress);

    float animationProgressSuddenExp = std::pow(animationProgress, 0.4f);
    float animationProgressLateExp = std::pow(animationProgress, 1.0f);

    float buttonSize = 50.0f * globalScale;
    float borderSize = 2.0f * globalScale;
    float rounding = 16.0f * globalScale;
    float fontSize = 12.0f * globalScale;
    float textOffset = 5.0f * globalScale;

    float buttonSpacing = 30.0f * globalScale;
    int numButtons = 4;
    float totalWidth = numButtons * buttonSize + (numButtons - 1) * buttonSpacing;

    ImDrawList *drawList = ImGui::GetBackgroundDrawList(viewport);

    float shadowWidth = 920.0f * globalScale;
    float shadowHeight = 135.0f * globalScale;
    float shadowYOffset = std::lerp(-shadowHeight / 2.0f, 0.0f, animationProgressSuddenExp);
    float overflow = std::max(0.0f, shadowWidth - viewport->Size.x) / 2.0f;
    float u0 = overflow / shadowWidth;
    float u1 = std::min(1.0f, (overflow + viewport->Size.x) / shadowWidth);
    drawList->AddImage(
        shadowTexture.id,
        ImVec2(viewport->Pos.x, viewport->Pos.y + shadowYOffset),
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + shadowHeight + shadowYOffset),
        ImVec2(u0, 0.0f), ImVec2(u1, 1.0f),
        IM_COL32(255, 255, 255, static_cast<uint8_t>(255.0f * animationProgressLateExp)));

    float xOffset = viewport->Pos.x + (viewport->Size.x - totalWidth) / 2;
    float yOffset = viewport->Pos.y + 20.0f * globalScale + shadowYOffset;

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoBringToFrontOnFocus |
                             ImGuiWindowFlags_NoNav |
                             ImGuiWindowFlags_NoDocking;

    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(xOffset, yOffset));
    ImGui::SetNextWindowSize(ImVec2(buttonSize, buttonSize));
    ImGui::Begin("topui.mainmenu", nullptr, flags);
    ImGui::SetCursorPos(ImVec2(0, 0));
    ImDrawList *draw = ImGui::GetWindowDrawList();
    draw->PushClipRectFullScreen();

    if (IconButton(font, "##mainmenu", "Main Menu", mainMenuTexture.id, buttonSize, borderSize, rounding, fontSize, textOffset, animationProgressLateExp) && !exitingFlag)
    {
        exitingFlag = true;
    }

    draw->PopClipRect();
    ImGui::End();

    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(xOffset + buttonSize + buttonSpacing, yOffset));
    ImGui::SetNextWindowSize(ImVec2(buttonSize, buttonSize));
    ImGui::Begin("topui.settings", nullptr, flags);
    ImGui::SetCursorPos(ImVec2(0, 0));
    draw = ImGui::GetWindowDrawList();
    draw->PushClipRectFullScreen();

    static bool showSettings = false;
    if (IconButton(font, "##settings", "Settings", settingsTexture.id, buttonSize, borderSize, rounding, fontSize, textOffset, animationProgressLateExp, showSettings))
    {
        showSettings = !showSettings;
    }

    draw->PopClipRect();
    ImGui::End();

    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(xOffset + (buttonSize + buttonSpacing) * 2, yOffset));
    ImGui::SetNextWindowSize(ImVec2(buttonSize, buttonSize));
    ImGui::Begin("topui.viewmode", nullptr, flags);
    ImGui::SetCursorPos(ImVec2(0, 0));
    draw = ImGui::GetWindowDrawList();
    draw->PushClipRectFullScreen();

    static bool showViewMode = false;
    if (IconButton(font, "##viewmode", "View Mode", viewModeTexture.id, buttonSize, borderSize, rounding, fontSize, textOffset, animationProgressLateExp, showViewMode))
    {
        showViewMode = !showViewMode;
    }

    draw->PopClipRect();
    ImGui::End();

    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::SetNextWindowPos(ImVec2(xOffset + (buttonSize + buttonSpacing) * 3, yOffset));
    ImGui::SetNextWindowSize(ImVec2(buttonSize, buttonSize));
    ImGui::Begin("topui.restartjava", nullptr, flags);
    ImGui::SetCursorPos(ImVec2(0, 0));
    draw = ImGui::GetWindowDrawList();
    draw->PushClipRectFullScreen();

    if (IconButton(font, "##restartjava", "Restart Java", restartJavaTexture.id, buttonSize, borderSize, rounding, fontSize, textOffset, animationProgressLateExp))
    {
        if (restartSimulationCallback)
        {
            restartSimulationCallback();
        }
    }

    draw->PopClipRect();
    ImGui::End();
}

void FieldRenderer::render(const blackboard::app::Window &window, const std::shared_ptr<Discord> &discord)
{
    currentDataUpdateIndex = (currentDataUpdateIndex + 1) % 1000000;
    if (settings::enableDebugMenu)
    {
        drawDebugMenu();
    }

    if (fmsUI)
    {
        fmsUI->render(ImGui::GetMainViewport()->Size);
    }

    drawTopUI();

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
        Mesh::createBuffersForMeshes(aprilTagMesh);
        fmsUI->postProcessField(fieldMeshes);

        createdFieldMeshBuffers = true;
    }

    if (!createdRobotMeshBuffers && robotModelLoadingFuture.valid() &&
        robotModelLoadingFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        if (robotModels.size() > 0)
        {
            addRobot(
                "/AdvantageKit/RealOutputs/RobotModel/Robot",
                "/AdvantageKit/RealOutputs/RobotModel/MechanismPoses",
                "/AdvantageKit/SystemStats/RSLState",
                "/AdvantageKit/DriverStation/AllianceStation",
                "/AdvantageKit/RealOutputs/LedManager/HexStrings");

            for (size_t i = 0; i < 6; ++i)
            {
                addRobot(
                    "/AdvantageKit/RealOutputs/OpponentRobot/" + std::to_string(i) + "/Pose",
                    "/AdvantageKit/RealOutputs/OpponentRobot/" + std::to_string(i) + "/MechanismPoses",
                    "/AdvantageKit/SystemStats/RSLState",
                    "/AdvantageKit/RealOutputs/OpponentRobot/" + std::to_string(i) + "/AllianceStation",
                    "/AdvantageKit/RealOutputs/LedManager/HexStrings",
                    "/AdvantageKit/RealOutputs/OpponentRobot/" + std::to_string(i) + "/Enabled");
            }
        }

        for (auto &model : robotModels)
        {
            Mesh::createBuffersForMeshes(model.meshes);
            for (size_t i = 0; i < model.components.size(); i++)
            {
                Mesh::createBuffersForMeshes(model.components[i].meshes);
            }

            model.rslMaterial = Mesh::getTaggedMaterial(model.meshes, "rsl");
            if (model.rslMaterial != nullptr)
            {
                model.rslOnEmissionStrength = model.rslMaterial->emissionColor[3];
            }

            model.bumperMaterial = Mesh::getTaggedMaterial(model.meshes, "bumper");
            if (model.bumperMaterial != nullptr)
            {
                model.bumperModelColor = model.bumperMaterial->baseColor;
            }

            model.ledMaterial = Mesh::getTaggedMaterial(model.meshes, "led");
            if (model.ledMaterial != nullptr)
            {
                model.ledMaterial->texture = "led";
            }

            logger->info("Post-processed robot meshes for materials: rsl={},bumper={},led={}", (model.rslMaterial != nullptr), (model.bumperMaterial != nullptr), (model.ledMaterial != nullptr));
        }

        createdRobotMeshBuffers = true;
    }

    const float deltaTime = std::max(0.0f, std::min(0.3f, ImGui::GetIO().DeltaTime));
    curTime += deltaTime;

    // Dynamic objects

    auto coordTransform = [this](const frc::Pose3d &pose)
    { return transformPose3dToLocalCoordinates(pose); };

    for (auto &[robotModel, instances] : robots)
    {
        for (auto &robot : instances)
        {
            robot.update(currentDataUpdateIndex, deltaTime, freezeTemporalEffects, coordTransform);
        }
    }

    for (auto &gamePiece : gamePieces)
    {
        gamePiece.update(currentDataUpdateIndex, deltaTime, freezeTemporalEffects, coordTransform);
    }

    if (cameraView == CameraView::Robot || cameraView == CameraView::RobotRelative)
    {
        if (robotModels.size() > 0)
        {
            auto &robotInstances = robots[&robotModels[0]];
            if (robotInstances.size() > 0 && robotInstances[0].dynamicData.lastDataUpdate == currentDataUpdateIndex)
            {
                float translationMtx[16];
                bx::mtxTranslate(translationMtx, robotInstances[0].dynamicData.lastPosition.x, robotInstances[0].dynamicData.lastPosition.y, robotInstances[0].dynamicData.lastPosition.z);
                if (cameraView == CameraView::RobotRelative)
                {
                    // also apply rotation
                    float rotationMtx[16];
                    bx::mtxFromQuaternion(rotationMtx, robotInstances[0].dynamicData.lastRotation);
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
    }

    uint16_t m_width = window.width;
    uint16_t m_height = window.height;

    float view[16];
    int allianceStation = 0;

    if (robotModels.size() > 0)
    {
        auto &robotInstances = robots[&robotModels[0]];
        if (robotInstances.size() > 0 && robotInstances[0].allianceStationSub.Exists())
        {
            allianceStation = robotInstances[0].allianceStationSub.GetAtomic().value;
            if (cameraView == CameraView::DriverStation)
            {
                // 6 elements ordered [B1, B2, B3, R1, R2, R3]
                int stationIndex = allianceStation >= 1 && allianceStation <= 3 ? allianceStation + 2 : allianceStation >= 4 && allianceStation <= 6 ? allianceStation - 4
                                                                                                                                                     : 0 /* fallback to 0 */;
                const bx::Vec3 at = bx::add(robotInstances[0].dynamicData.lastPosition, bx::Vec3{0.0f, 0.0f, 0.5f});
                const bx::Vec3 eye = driverStationCameraPositions[stationIndex];

                const bx::Vec3 velocity = bx::sub(at, lastDriverStationCameraTarget);
                lastDriverStationCameraTarget = at;

                float cameraDistance = bx::distance(eye, at);
                float targetDistance = bx::distance(driverStationCameraTarget, at) / cameraDistance;
                if (targetDistance > 0.2f)
                {
                    driverStationCameraTarget = bx::lerp(driverStationCameraTarget, at, 6.0f * (targetDistance - 0.2f) * std::max(deltaTime, std::min(0.2f, bx::length(velocity))));
                }

                bx::mtxLookAt(view, eye, driverStationCameraTarget, {0.0f, 0.0f, 1.0f}, bx::Handedness::Right);
            }
            else
            {
                updateOrbitCameraFromInput();
                const bx::Vec3 at = orbitCamera.target;
                const bx::Vec3 eye = orbitCamera.getEye();

                driverStationCameraTarget = at;
                lastDriverStationCameraTarget = at;

                float lookAt[16];
                bx::mtxLookAt(lookAt, eye, at, {0.0f, 0.0f, 1.0f}, bx::Handedness::Right);
                bx::mtxMul(view, orbitCamera.originTransform, lookAt);
            }
        }
        else
        {
            updateOrbitCameraFromInput();
            const bx::Vec3 at = orbitCamera.target;
            const bx::Vec3 eye = orbitCamera.getEye();

            driverStationCameraTarget = at;
            lastDriverStationCameraTarget = at;

            float lookAt[16];
            bx::mtxLookAt(lookAt, eye, at, {0.0f, 0.0f, 1.0f}, bx::Handedness::Right);
            bx::mtxMul(view, orbitCamera.originTransform, lookAt);
        }
    }
    else
    {
        updateOrbitCameraFromInput();
        const bx::Vec3 at = orbitCamera.target;
        const bx::Vec3 eye = orbitCamera.getEye();

        driverStationCameraTarget = at;
        lastDriverStationCameraTarget = at;

        float lookAt[16];
        bx::mtxLookAt(lookAt, eye, at, {0.0f, 0.0f, 1.0f}, bx::Handedness::Right);
        bx::mtxMul(view, orbitCamera.originTransform, lookAt);
    }

    auto now = std::chrono::high_resolution_clock::now();
    if (now - lastDiscordUpdateTime > DISCORD_UPDATE_INTERVAL)
    {
        lastDiscordUpdateTime = now;
        discord->setField(
            GAME_YEAR,
            allianceStation,
            fmsUI ? fmsUI->getDriverScore() : 0,
            fmsUI ? fmsUI->getOpponentScore() : 0,
            robotModels.size() > 0 ? robotModels[0].name : "<Unknown>",
            fmsUI ? fmsUI->getDriveMode() : "<Unknown>",
            "https://github.com/recordrobotics/2026-robot",
            "https://github.com/recordrobotics/2026-robot/releases/latest",
            fmsUI ? fmsUI->getMatchEndTime() : 0);
    }

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

    updateInfo(0.1f, proj);

    if (!freezeTemporalEffects)
    {
        jitterIndex = (jitterIndex + 1) % HALTON_SAMPLES;
    }

    float jitterData[4] = {jitterX, jitterY, previousJitterX, previousJitterY};
    bgfx::setFrameUniform(u_jitter, jitterData);

    bgfx::setFrameUniform(u_previousViewProj, previousViewProj);
    bgfx::setFrameUniform(u_previousView, previousView);
    bgfx::setFrameUniform(u_previousProj, previousProj);

    // Light uniforms
    Vec3Padded lightPos[LIGHT_COUNT] = {
        Vec3Padded(bx::mul({-fieldWidthMeters / 2.6f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({-fieldWidthMeters / 2.6f, fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({0.0f, fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({0.0f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({fieldWidthMeters / 2.6f, -fieldHeightMeters / 2.5f, 6.0f}, view)),
        Vec3Padded(bx::mul({fieldWidthMeters / 2.6f, fieldHeightMeters / 2.5f, 6.0f}, view))};

    bgfx::setFrameUniform(u_lightPos, lightPos, LIGHT_COUNT);
    bgfx::setFrameUniform(u_lightColor, lightColor.data(), LIGHT_COUNT);

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
    bgfx::setViewClear(VIEW_OIT,
                       BGFX_CLEAR_COLOR,
                       0.0f,
                       0,
                       0);

    // Transparent moments
    bgfx::setViewName(VIEW_OIT_MOMENTS, "Field - OIT Moments");
    bgfx::setViewTransform(VIEW_OIT_MOMENTS, view, proj);
    bgfx::setViewRect(VIEW_OIT_MOMENTS, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_OIT_MOMENTS, gOitMomentsFbo.handle);
    bgfx::setPaletteColor(0, 0, 0, 0, 0); // Clear moments and totalDepth to 0
    bgfx::setViewClear(VIEW_OIT_MOMENTS,
                       BGFX_CLEAR_COLOR,
                       0.0f,
                       0,
                       0,
                       0);

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

    bgfx::Encoder *encoder = bgfx::begin();

    if (createdFieldMeshBuffers)
    {
        std::vector<std::string> drawnGamePieces;
        for (auto &gamePiece : gamePieces)
        {
            std::vector<InstanceData> instanceData;
            auto filtered = gamePiece.instances | std::views::filter([this](const std::pair<int, DynamicObjectData> &pair)
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

        drawAprilTags(encoder);
    }

    if (createdRobotMeshBuffers)
    {
        for (auto &[robotModel, instances] : robots)
        {
            drawRobot(encoder, robotModel, instances | std::views::filter([this](const RobotData &robot)
                                                                          { return robot.dynamicData.lastDataUpdate == currentDataUpdateIndex; }),
                      [this](const DynamicObjectData &component)
                      { return component.lastDataUpdate == currentDataUpdateIndex; });
        }
    }

    // always clear OIT buffers
    encoder->touch(VIEW_OIT);
    encoder->touch(VIEW_OIT_MOMENTS);

    int xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    int yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

    // GTAO
    if (settings::enableGTAO)
    {
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
            gtaoDenoiseBlurBeta,
            0.0f};

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

        xGroups = (int)floorf(((float)m_width - 1) / 32 + 1); // denoise computes 2 horizontal pixels at a time
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setTexture(0, s_workingAOTerm, gGTAOWorkingAOTerm.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
        encoder->setTexture(1, s_workingEdges, gGTAOWorkingEdges.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
        encoder->setImage(2, gGTAOFinalAOTerm.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_GTAO, XeGTAO_DenoiseProgram, xGroups, yGroups);
    }

    // Post processing

    xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

    // OIT Composition

    float gtaoIntensityField[4] = {
        settings::enableGTAO ? gtaoIntensity : 0.0f,
        settings::enableGTAO ? (gtaoIntensity * gtaoDirectIntensity) : 0.0f,
        settings::enableGTAO ? (gtaoIntensity * gtaoBentNormalIntensity) : 0.0f,
        0.0f};

    encoder->setUniform(u_skyColor, skyColor.data());
    encoder->setUniform(u_gtaoIntensity, gtaoIntensityField);
    encoder->setImage(0, gAccumTex.handle, 0, bgfx::Access::Read);
    encoder->setImage(1, gbufAlbedo.handle, 0, bgfx::Access::Read);
    encoder->setImage(2, gbufEmission.handle, 0, bgfx::Access::Read);
    encoder->setImage(3, gbufNormal.handle, 0, bgfx::Access::Read);
    encoder->setImage(4, gbufPBRData.handle, 0, bgfx::Access::Read);
    encoder->setImage(5, gGTAOFinalAOTerm.handle, 0, bgfx::Access::Read);
    encoder->setImage(6, gOutputColor.handle, 0, bgfx::Access::Write);
    encoder->setTexture(7, s_depth, gbufDepth.handle, BGFX_SAMPLER_POINT | BGFX_SAMPLER_UVW_CLAMP);
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
    encoder->setTexture(0, s_depth, gbufDepth.handle);
    encoder->setTexture(1, s_velocity, gbufVelocity.handle);
    encoder->setImage(2, gMBVelocity.handle, 0, bgfx::Access::Write);
    encoder->setImage(3, gFullVelocity.handle, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_POSTPROCESS, mbVelocityProgram, xGroups, yGroups);

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

    // Exposure
    xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);
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
            encoder->setTexture(0, s_bloomInput, gOutputColor.handle, 0, 1, i, 1);

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
            encoder->setTexture(0, s_bloomInput, gOutputColor.handle, 0, 1, i, 1);
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

    // Tonemap
    xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
    yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);
    encoder->setUniform(u_lutParams, lutParams);
    encoder->setImage(0, gOutputColor.handle, 0, bgfx::Access::ReadWrite);
    encoder->setTexture(1, s_lut, tonemappingLut.handle);
    encoder->dispatch(VIEW_POSTPROCESS, tonemapProgram, xGroups, yGroups);

    // Blit and present

    switch (debugView)
    {
    case DebugView::Albedo:
        encoder->setTexture(0, s_texPresent, gbufAlbedo.handle);
        break;
    case DebugView::Normal:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gbufNormal.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, debugNormalsProgram, xGroups, yGroups);

        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    case DebugView::Emission:
        encoder->setTexture(0, s_texPresent, gbufEmission.handle);
        break;
    case DebugView::PBRData:
        encoder->setTexture(0, s_texPresent, gbufPBRData.handle);
        break;
    case DebugView::Velocity:
        encoder->setTexture(0, s_texPresent, gbufVelocity.handle);
        break;
    case DebugView::Depth:
        encoder->setTexture(0, s_texPresent, gbufDepth.handle);
        break;
    case DebugView::OITMoments:
        encoder->setTexture(0, s_texPresent, gMomentsTex.handle);
        break;
    case DebugView::OITTotalDepth:
        encoder->setTexture(0, s_texPresent, gTotalDepthTex.handle);
        break;
    case DebugView::OITAccum:
        encoder->setTexture(0, s_texPresent, gAccumTex.handle);
        break;
    case DebugView::MotionBlurVelocity:
        encoder->setTexture(0, s_texPresent, gMBVelocity.handle);
        break;
    case DebugView::MotionBlurTileMaxX:
        encoder->setTexture(0, s_texPresent, gMBTileMaxX.handle);
        break;
    case DebugView::MotionBlurTileMaxY:
        encoder->setTexture(0, s_texPresent, gMBTileMax.handle);
        break;
    case DebugView::MotionBlurNeighborMax:
        encoder->setTexture(0, s_texPresent, gMBNeighborMax.handle);
        break;
    case DebugView::MotionBlurTileVariance:
        encoder->setTexture(0, s_texPresent, gMBTileVariance.handle);
        break;
    case DebugView::GTAOWorkingDepth0:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingDepth.handle, 0, 1, 0, 1);
        break;
    case DebugView::GTAOWorkingDepth1:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingDepth.handle, 0, 1, 1, 1);
        break;
    case DebugView::GTAOWorkingDepth2:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingDepth.handle, 0, 1, 2, 1);
        break;
    case DebugView::GTAOWorkingDepth3:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingDepth.handle, 0, 1, 3, 1);
        break;
    case DebugView::GTAOWorkingDepth4:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingDepth.handle, 0, 1, 4, 1);
        break;
    case DebugView::GTAOWorkingAOTermNormals:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOWorkingAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugNormalsProgram, xGroups, yGroups);

        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    case DebugView::GTAOWorkingAOTermVisibility:
        // Unpack and store visibility in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOWorkingAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugVisibilityProgram, xGroups, yGroups);

        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    case DebugView::GTAOFinalAOTermNormals:
        // Unpack and store normals in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOFinalAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugNormalsProgram, xGroups, yGroups);

        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    case DebugView::GTAOFinalAOTermVisibility:
        // Unpack and store visibility in gOutputColor
        xGroups = (int)floorf(((float)m_width - 1) / 16 + 1);
        yGroups = (int)floorf(((float)m_height - 1) / 16 + 1);

        encoder->setImage(0, gGTAOFinalAOTerm.handle, 0, bgfx::Access::Read);
        encoder->setImage(1, gOutputColor.handle, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_BLIT, XeGTAO_debugVisibilityProgram, xGroups, yGroups);

        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    case DebugView::GTAOWorkingEdges:
        encoder->setTexture(0, s_texPresent, gGTAOWorkingEdges.handle);
        break;
    default:
        encoder->setTexture(0, s_texPresent, gOutputColor.handle);
        break;
    }

    encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
    screenSpaceQuad(!bgfx::getCaps()->originBottomLeft, encoder);
    encoder->submit(VIEW_BLIT, presentProgram);

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

bool FieldRenderer::isExiting()
{
    return exitingFlag;
}

FieldRenderer::~FieldRenderer()
{
    exitingFlag = false;

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

    for (auto &model : robotModels)
    {
        for (auto &mesh : model.meshes)
        {
            mesh.destroy();
        }

        for (auto &component : model.components)
        {
            for (auto &mesh : component.meshes)
            {
                mesh.destroy();
            }
        }
    }

    for (auto &[robotModel, instances] : robots)
    {
        for (auto &robot : instances)
        {
            robot.ledColorTexture.destroy();
        }
    }

    aprilTagMesh.destroy();

    if (bgfx::isValid(u_baseColor))
        bgfx::destroy(u_baseColor);
    if (bgfx::isValid(u_emissionColor))
        bgfx::destroy(u_emissionColor);
    if (bgfx::isValid(u_skyColor))
        bgfx::destroy(u_skyColor);
    if (bgfx::isValid(u_gtaoIntensity))
        bgfx::destroy(u_gtaoIntensity);
    if (bgfx::isValid(u_info))
        bgfx::destroy(u_info);
    if (bgfx::isValid(u_previousViewProj))
        bgfx::destroy(u_previousViewProj);
    if (bgfx::isValid(u_previousView))
        bgfx::destroy(u_previousView);
    if (bgfx::isValid(u_previousProj))
        bgfx::destroy(u_previousProj);
    if (bgfx::isValid(u_jitter))
        bgfx::destroy(u_jitter);
    if (bgfx::isValid(u_pbrData))
        bgfx::destroy(u_pbrData);
    if (bgfx::isValid(u_ledData))
        bgfx::destroy(u_ledData);
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
    if (bgfx::isValid(programPBRLed))
        bgfx::destroy(programPBRLed);
    if (bgfx::isValid(programPBRApriltag))
        bgfx::destroy(programPBRApriltag);
    if (bgfx::isValid(programOit))
        bgfx::destroy(programOit);
    if (bgfx::isValid(programOitInstanced))
        bgfx::destroy(programOitInstanced);
    if (bgfx::isValid(programOitMoments))
        bgfx::destroy(programOitMoments);
    if (bgfx::isValid(programOitMomentsInstanced))
        bgfx::destroy(programOitMomentsInstanced);
    if (bgfx::isValid(oitCompProgram))
        bgfx::destroy(oitCompProgram);
    if (bgfx::isValid(tonemapProgram))
        bgfx::destroy(tonemapProgram);
    if (bgfx::isValid(exposureProgram))
        bgfx::destroy(exposureProgram);
    if (bgfx::isValid(blitProgram))
        bgfx::destroy(blitProgram);
    if (bgfx::isValid(presentProgram))
        bgfx::destroy(presentProgram);
    if (bgfx::isValid(debugNormalsProgram))
        bgfx::destroy(debugNormalsProgram);
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
    if (bgfx::isValid(XeGTAO_DenoiseProgram))
        bgfx::destroy(XeGTAO_DenoiseProgram);
    if (bgfx::isValid(XeGTAO_debugNormalsProgram))
        bgfx::destroy(XeGTAO_debugNormalsProgram);
    if (bgfx::isValid(XeGTAO_debugVisibilityProgram))
        bgfx::destroy(XeGTAO_debugVisibilityProgram);

    gAccumTex.destroy();
    gMomentsTex.destroy();
    gTotalDepthTex.destroy();

    gOitFbo.destroy();
    gOitMomentsFbo.destroy();

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
    gGTAOFinalAOTerm.destroy();

    bloomDirtMask.destroy();

    tonemappingLut.destroy();

    carpetBaseColor.destroy();
    carpetBump.destroy();
    apriltagTexture.destroy();
    ledMaskTexture.destroy();

    if (bgfx::isValid(s_texPresent))
        bgfx::destroy(s_texPresent);

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

    if (bgfx::isValid(s_bloomInput))
        bgfx::destroy(s_bloomInput);
    if (bgfx::isValid(s_bloomDirt))
        bgfx::destroy(s_bloomDirt);

    if (bgfx::isValid(s_lut))
        bgfx::destroy(s_lut);

    if (bgfx::isValid(s_baseColor))
        bgfx::destroy(s_baseColor);
    if (bgfx::isValid(s_bump))
        bgfx::destroy(s_bump);
    if (bgfx::isValid(s_apriltags))
        bgfx::destroy(s_apriltags);
    if (bgfx::isValid(s_ledMask))
        bgfx::destroy(s_ledMask);
    if (bgfx::isValid(s_ledColors))
        bgfx::destroy(s_ledColors);
    if (bgfx::isValid(s_momentsTex))
        bgfx::destroy(s_momentsTex);
    if (bgfx::isValid(s_totalDepthTex))
        bgfx::destroy(s_totalDepthTex);

    if (bgfx::isValid(s_workingAOTerm))
        bgfx::destroy(s_workingAOTerm);
    if (bgfx::isValid(s_workingEdges))
        bgfx::destroy(s_workingEdges);

    shadowTexture.destroy();
    mainMenuTexture.destroy();
    settingsTexture.destroy();
    viewModeTexture.destroy();
    restartJavaTexture.destroy();
}