#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <SDL3/SDL.h>
#include <bx/file.h>
#include <bx/error.h>
#include <bx/pixelformat.h>
#include <imgui/imgui.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <vector>
#include <algorithm>
#include <cstring>
#include <cmath>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include <blackboard_app/logger.h>

#include "fieldrenderer.h"
#include "shaders.h"

using namespace blackboard::logger;

static constexpr uint16_t VIEW_GBUFFER = 0;
static constexpr uint16_t VIEW_OIT = 1;
static constexpr uint16_t VIEW_OIT_DEPTH_POST_PASS = 2;
static constexpr uint16_t VIEW_POSTPROCESS = 3;
static constexpr uint16_t VIEW_BLIT = 4;

bx::DefaultAllocator allocator;
bx::FileReader reader;
bx::Error err;

bgfx::UniformHandle u_baseColor;
bgfx::UniformHandle u_info;
bgfx::UniformHandle u_previousModelViewProj;
bgfx::UniformHandle u_jitter;

bgfx::ProgramHandle program = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOit = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle programOitDepthPostPass = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle oitCompProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle blitProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle cameraVelocityProgram = BGFX_INVALID_HANDLE;
bgfx::ProgramHandle taaResolveProgram = BGFX_INVALID_HANDLE;

bgfx::TextureHandle gAccumTex;
bgfx::TextureHandle gRevealTex;
bgfx::TextureHandle gbufAlbedo;
bgfx::TextureHandle gbufNormal;
bgfx::TextureHandle gbufVelocity;
bgfx::TextureHandle gbufDepth;
bgfx::TextureHandle gFXSwap;
bgfx::TextureHandle gTAABuffer0;
bgfx::TextureHandle gTAABuffer1;
bgfx::FrameBufferHandle gBufFbo;
bgfx::FrameBufferHandle gOitFbo;
bgfx::FrameBufferHandle gOitDepthPostPassFbo;
bgfx::UniformHandle s_tex;
bgfx::UniformHandle s_taaHistory;

void initOIT(uint16_t width, uint16_t height)
{
    gAccumTex = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(gAccumTex))
    {
        logger->error("Failed to create accumulation texture for OIT.");
        throw std::runtime_error("Failed to create accumulation texture for OIT.");
    }

    gRevealTex = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::R16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(gRevealTex))
    {
        logger->error("Failed to create revealage texture for OIT.");
        throw std::runtime_error("Failed to create revealage texture for OIT.");
    }

    bgfx::TextureHandle attachments[] =
        {
            gAccumTex,
            gRevealTex,
            gbufDepth};

    gOitFbo = bgfx::createFrameBuffer(
        BX_COUNTOF(attachments),
        attachments,
        true);

    if (!bgfx::isValid(gOitFbo))
    {
        logger->error("Failed to create framebuffer for OIT.");
        throw std::runtime_error("Failed to create framebuffer for OIT.");
    }

    bgfx::TextureHandle depthPostPassAttachments[] =
        {
            gbufVelocity,
            gbufDepth};

    gOitDepthPostPassFbo = bgfx::createFrameBuffer(
        BX_COUNTOF(depthPostPassAttachments),
        depthPostPassAttachments,
        true);

    if (!bgfx::isValid(gOitDepthPostPassFbo))
    {
        logger->error("Failed to create framebuffer for OIT Depth Post-Pass.");
        throw std::runtime_error("Failed to create framebuffer for OIT Depth Post-Pass.");
    }

    gFXSwap = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    if (!bgfx::isValid(gFXSwap))
    {
        logger->error("Failed to create swap texture.");
        throw std::runtime_error("Failed to create swap texture.");
    }
}

void initTAA(uint16_t width, uint16_t height)
{
    gTAABuffer0 = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_COMPUTE_WRITE);

    if (!bgfx::isValid(gTAABuffer0))
    {
        logger->error("Failed to create TAA buffer 0 texture.");
        throw std::runtime_error("Failed to create TAA buffer 0 texture.");
    }

    gTAABuffer1 = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_COMPUTE_WRITE);

    if (!bgfx::isValid(gTAABuffer1))
    {
        logger->error("Failed to create TAA buffer 1 texture.");
        throw std::runtime_error("Failed to create TAA buffer 1 texture.");
    }
}

void initGBuffer(uint16_t width, uint16_t height)
{
    gbufAlbedo = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(gbufAlbedo))
    {
        logger->error("Failed to create albedo texture for G-buffer.");
        throw std::runtime_error("Failed to create albedo texture for G-buffer.");
    }

    gbufNormal = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RGBA8S,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(gbufNormal))
    {
        logger->error("Failed to create normal texture for G-buffer.");
        throw std::runtime_error("Failed to create normal texture for G-buffer.");
    }

    gbufVelocity = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::RG16F,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_TEXTURE_COMPUTE_WRITE);

    if (!bgfx::isValid(gbufVelocity))
    {
        logger->error("Failed to create velocity texture for G-buffer.");
        throw std::runtime_error("Failed to create velocity texture for G-buffer.");
    }

    gbufDepth = bgfx::createTexture2D(
        width, height,
        false,
        1,
        bgfx::TextureFormat::D24S8,
        BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);

    if (!bgfx::isValid(gbufDepth))
    {
        logger->error("Failed to create depth texture for G-buffer.");
        throw std::runtime_error("Failed to create depth texture for G-buffer.");
    }

    bgfx::TextureHandle attachments[] =
        {
            gbufAlbedo,
            gbufNormal,
            gbufVelocity,
            gbufDepth};

    gBufFbo = bgfx::createFrameBuffer(
        BX_COUNTOF(attachments),
        attachments,
        true);

    if (!bgfx::isValid(gBufFbo))
    {
        logger->error("Failed to create framebuffer for G-buffer.");
        throw std::runtime_error("Failed to create framebuffer for G-buffer.");
    }
}

static const bgfx::EmbeddedShader s_embeddedShaders[] =
    {
        BGFX_EMBEDDED_SHADER(vs_pbr),
        BGFX_EMBEDDED_SHADER(fs_pbr),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit),
        BGFX_EMBEDDED_SHADER(fs_pbr_oit_depth_post_pass),

        BGFX_EMBEDDED_SHADER(vs_pass),
        BGFX_EMBEDDED_SHADER(fs_blit),

        BGFX_EMBEDDED_SHADER(cs_oit_comp),
        BGFX_EMBEDDED_SHADER(cs_taa_resolve),
        BGFX_EMBEDDED_SHADER(cs_camera_velocity),

        BGFX_EMBEDDED_SHADER_END()};

struct MaterialGPU
{
    float baseColor[4]; // includes alpha
    bool transparent;
};

struct MeshBatch
{
    bgfx::VertexBufferHandle vbh;
    bgfx::IndexBufferHandle ibh;
    uint32_t indexCount;
    float baseColor[4];
};

struct MaterialKey
{
    uint32_t baseColor[4];

    bool operator==(const MaterialKey &other) const
    {
        return baseColor[0] == other.baseColor[0] &&
               baseColor[1] == other.baseColor[1] &&
               baseColor[2] == other.baseColor[2] &&
               baseColor[3] == other.baseColor[3];
    }
};

struct MaterialKeyHash
{
    std::size_t operator()(const MaterialKey &key) const
    {
        std::size_t h = 2166136261u;
        for (std::size_t i = 0; i < 4; ++i)
        {
            h ^= key.baseColor[i];
            h *= 16777619u;
        }
        return h;
    }
};

/// Encode normal vector as RGBA8 color value.
///
/// @param[in] _x X component of the normal.
/// @param[in] _y Y component of the normal.
/// @param[in] _z Z component of the normal.
/// @param[in] _w W component.
///
/// @returns Packed RGBA8 value.
///
inline uint32_t encodeNormalRgba8(float _x, float _y = 0.0f, float _z = 0.0f, float _w = 0.0f)
{
    const float src[] =
        {
            _x * 0.5f + 0.5f,
            _y * 0.5f + 0.5f,
            _z * 0.5f + 0.5f,
            _w * 0.5f + 0.5f,
        };
    uint32_t dst;
    bx::packRgba8(&dst, src);
    return dst;
}

struct Vertex
{
    float x, y, z;
    uint32_t normal;

    static bgfx::VertexLayout layout;

    static void init()
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
            .end();
    }
};

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

bgfx::VertexLayout Vertex::layout;
bgfx::VertexLayout UVVertex::layout;

struct BatchBuildData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    MaterialGPU material;
};

std::vector<MeshBatch> opaqueBatches;
std::vector<MeshBatch> transparentBatches;

struct OrbitCamera
{
    bx::Vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 15.811388f;
    float yaw = 3.14159265f;
    float pitch = 0.32175055f;
    float minDistance = 2.0f;
    float maxDistance = 120.0f;
    float orbitSpeed = 0.008f;
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

        constexpr float pitchLimit = 1.55334306f; // ~89 degrees in radians.
        orbitCamera.pitch = std::clamp(orbitCamera.pitch, -pitchLimit, pitchLimit);
    }
}

bx::Vec3 getOrbitEye()
{
    const float cosPitch = std::cos(orbitCamera.pitch);
    const float sinPitch = std::sin(orbitCamera.pitch);
    const float sinYaw = std::sin(orbitCamera.yaw);
    const float cosYaw = std::cos(orbitCamera.yaw);

    return {
        orbitCamera.target.x + orbitCamera.distance * cosPitch * sinYaw,
        orbitCamera.target.y + orbitCamera.distance * sinPitch,
        orbitCamera.target.z + orbitCamera.distance * cosPitch * cosYaw,
    };
}

uint32_t floatToBits(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

MaterialKey makeMaterialKey(const MaterialGPU &material)
{
    return {
        {
            floatToBits(material.baseColor[0]),
            floatToBits(material.baseColor[1]),
            floatToBits(material.baseColor[2]),
            floatToBits(material.baseColor[3]),
        }};
}

MaterialGPU convertMaterial(const fastgltf::Material &m)
{
    MaterialGPU out{};

    auto &base = m.pbrData.baseColorFactor;

    out.baseColor[0] = base[0];
    out.baseColor[1] = base[1];
    out.baseColor[2] = base[2];
    out.baseColor[3] = base[3];

    out.transparent =
        m.alphaMode == fastgltf::AlphaMode::Blend ||
        out.baseColor[3] < 0.999f;

    return out;
}

void field::init(const blackboard::app::Window &window)
{
    Vertex::init();
    UVVertex::init();

    s_tex = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

    if (!bgfx::isValid(s_tex))
    {
        logger->error("Failed to create uniform for generic texture.");
        throw std::runtime_error("Failed to create uniform for generic texture.");
    }

    s_taaHistory = bgfx::createUniform("s_taaHistory", bgfx::UniformType::Sampler);
    if (!bgfx::isValid(s_taaHistory))
    {
        logger->error("Failed to create uniform for TAA history texture.");
        throw std::runtime_error("Failed to create uniform for TAA history texture.");
    }

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

        std::string fieldModelFile = fieldDirectory + "model.glb";
        fastgltf::Parser parser;

        auto gltfData = fastgltf::GltfDataBuffer::FromPath(fieldModelFile);

        auto asset = parser.loadGltfBinary(
            gltfData.get(),
            fieldDirectory,
            fastgltf::Options::LoadGLBBuffers |
                fastgltf::Options::DontRequireValidAssetMember);

        const std::size_t sceneIndex = asset->defaultScene.value_or(0);

        std::unordered_map<MaterialKey, BatchBuildData, MaterialKeyHash> opaqueBatchMap;
        std::unordered_map<MaterialKey, BatchBuildData, MaterialKeyHash> transparentBatchMap;

        fastgltf::iterateSceneNodes(asset.get(), sceneIndex, fastgltf::math::fmat4x4(), [&](fastgltf::Node &node, const fastgltf::math::fmat4x4 &worldMatrix)
                                    {
            if (!node.meshIndex.has_value() || node.meshIndex.value() >= asset->meshes.size())
            {
                return;
            }

            auto &mesh = asset->meshes[node.meshIndex.value()];
            for (auto &primitive : mesh.primitives)
            {
                auto positionIt = primitive.findAttribute("POSITION");
                if (positionIt == primitive.attributes.end())
                {
                    logger->warn("Skipping primitive without POSITION attribute.");
                    continue;
                }

                if (!primitive.indicesAccessor.has_value())
                {
                    logger->warn("Skipping primitive without indices accessor.");
                    continue;
                }
                auto &positionAccessor = asset->accessors[positionIt->accessorIndex];
                auto& indicesAccessor = asset->accessors[primitive.indicesAccessor.value()];

                if (positionAccessor.count == 0 || indicesAccessor.count == 0)
                {
                    logger->warn("Skipping primitive with empty geometry data.");
                    continue;
                }

                MaterialGPU mat{{1.0f, 1.0f, 1.0f, 1.0f}, false};
                if (primitive.materialIndex.has_value() && primitive.materialIndex.value() < asset->materials.size())
                {
                    mat = convertMaterial(asset->materials[primitive.materialIndex.value()]);
                }

                const MaterialKey key = makeMaterialKey(mat);
                auto &batch = mat.transparent ? transparentBatchMap[key] : opaqueBatchMap[key];
                if (batch.vertices.empty())
                {
                    batch.material = mat;
                }

                const uint32_t baseVertex = static_cast<uint32_t>(batch.vertices.size());
                batch.vertices.reserve(batch.vertices.size() + positionAccessor.count);
                batch.indices.reserve(batch.indices.size() + indicesAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset.get(),
                    positionAccessor,
                    [&](fastgltf::math::fvec3 pos, std::size_t index)
                    {
                        fastgltf::math::fvec4 worldPos = worldMatrix * fastgltf::math::fvec4(pos.x(), pos.y(), pos.z(), 1.0f);
                        Vertex v{};
                        v.x = worldPos.x();
                        v.y = worldPos.y();
                        v.z = worldPos.z();
                        v.normal = 0;
                        batch.vertices.push_back(v);
                    });

                auto normalIt = primitive.findAttribute("NORMAL");
                if (normalIt != primitive.attributes.end())
                {
                    auto &normalAccessor = asset->accessors[normalIt->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset.get(),
                        normalAccessor,
                        [&](fastgltf::math::fvec3 norm, std::size_t index)
                        {
                            fastgltf::math::fvec4 worldNorm = worldMatrix * fastgltf::math::fvec4(norm.x(), norm.y(), norm.z(), 0.0f);
                            batch.vertices[baseVertex + index].normal = encodeNormalRgba8(worldNorm.x(), worldNorm.y(), worldNorm.z());
                        });
                }
                    
                fastgltf::iterateAccessor<uint32_t>(
                    asset.get(),
                    indicesAccessor,
                    [&](uint32_t idx)
                    {
                        batch.indices.push_back(baseVertex + idx);
                    });
            } });

        opaqueBatches.reserve(opaqueBatchMap.size());
        transparentBatches.reserve(transparentBatchMap.size());

        for (auto &entry : opaqueBatchMap)
        {
            auto &batchData = entry.second;
            if (batchData.vertices.empty() || batchData.indices.empty())
            {
                continue;
            }

            MeshBatch gpuBatch;
            gpuBatch.vbh = bgfx::createVertexBuffer(
                bgfx::copy(batchData.vertices.data(), static_cast<uint32_t>(batchData.vertices.size() * sizeof(Vertex))),
                Vertex::layout);

            if (!bgfx::isValid(gpuBatch.vbh))
            {
                logger->error("Failed to create vertex buffer for opaque batch.");
                throw std::runtime_error("Failed to create vertex buffer for opaque batch.");
            }

            gpuBatch.ibh = bgfx::createIndexBuffer(
                bgfx::copy(batchData.indices.data(), static_cast<uint32_t>(batchData.indices.size() * sizeof(uint32_t))),
                BGFX_BUFFER_INDEX32);

            if (!bgfx::isValid(gpuBatch.ibh))
            {
                logger->error("Failed to create index buffer for opaque batch.");
                throw std::runtime_error("Failed to create index buffer for opaque batch.");
            }

            gpuBatch.indexCount = static_cast<uint32_t>(batchData.indices.size());
            gpuBatch.baseColor[0] = batchData.material.baseColor[0];
            gpuBatch.baseColor[1] = batchData.material.baseColor[1];
            gpuBatch.baseColor[2] = batchData.material.baseColor[2];
            gpuBatch.baseColor[3] = batchData.material.baseColor[3];

            opaqueBatches.push_back(gpuBatch);
        }

        for (auto &entry : transparentBatchMap)
        {
            auto &batchData = entry.second;
            if (batchData.vertices.empty() || batchData.indices.empty())
            {
                continue;
            }

            MeshBatch gpuBatch;
            gpuBatch.vbh = bgfx::createVertexBuffer(
                bgfx::copy(batchData.vertices.data(), static_cast<uint32_t>(batchData.vertices.size() * sizeof(Vertex))),
                Vertex::layout);

            if (!bgfx::isValid(gpuBatch.vbh))
            {
                logger->error("Failed to create vertex buffer for transparent batch.");
                throw std::runtime_error("Failed to create vertex buffer for transparent batch.");
            }

            gpuBatch.ibh = bgfx::createIndexBuffer(
                bgfx::copy(batchData.indices.data(), static_cast<uint32_t>(batchData.indices.size() * sizeof(uint32_t))),
                BGFX_BUFFER_INDEX32);

            if (!bgfx::isValid(gpuBatch.ibh))
            {
                logger->error("Failed to create index buffer for transparent batch.");
                throw std::runtime_error("Failed to create index buffer for transparent batch.");
            }

            gpuBatch.indexCount = static_cast<uint32_t>(batchData.indices.size());
            gpuBatch.baseColor[0] = batchData.material.baseColor[0];
            gpuBatch.baseColor[1] = batchData.material.baseColor[1];
            gpuBatch.baseColor[2] = batchData.material.baseColor[2];
            gpuBatch.baseColor[3] = batchData.material.baseColor[3];

            transparentBatches.push_back(gpuBatch);
        }

        const auto type = bgfx::getRendererType();

        program =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                                bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr"), true);

        if (!bgfx::isValid(program))
        {
            logger->error("Failed to create main rendering program.");
            throw std::runtime_error("Failed to create main rendering program.");
        }

        programOit =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                                bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit"), true);

        if (!bgfx::isValid(programOit))
        {
            logger->error("Failed to create OIT rendering program.");
            throw std::runtime_error("Failed to create OIT rendering program.");
        }

        programOitDepthPostPass =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pbr"),
                                bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_pbr_oit_depth_post_pass"), true);

        if (!bgfx::isValid(programOitDepthPostPass))
        {
            logger->error("Failed to create OIT depth post-pass program.");
            throw std::runtime_error("Failed to create OIT depth post-pass program.");
        }

        oitCompProgram =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_oit_comp"), true);

        if (!bgfx::isValid(oitCompProgram))
        {
            logger->error("Failed to create OIT composition program.");
            throw std::runtime_error("Failed to create OIT composition program.");
        }

        blitProgram =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_pass"),
                                bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_blit"), true);

        if (!bgfx::isValid(blitProgram))
        {
            logger->error("Failed to create blit program.");
            throw std::runtime_error("Failed to create blit program.");
        }

        taaResolveProgram =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_taa_resolve"), true);

        if (!bgfx::isValid(taaResolveProgram))
        {
            logger->error("Failed to create TAA resolve program.");
            throw std::runtime_error("Failed to create TAA resolve program.");
        }

        cameraVelocityProgram =
            bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "cs_camera_velocity"), true);
        if (!bgfx::isValid(cameraVelocityProgram))
        {
            logger->error("Failed to create camera velocity program.");
            throw std::runtime_error("Failed to create camera velocity program.");
        }

        u_baseColor = bgfx::createUniform("u_baseColor", bgfx::UniformType::Vec4);

        if (!bgfx::isValid(u_baseColor))
        {
            logger->error("Failed to create uniform: u_baseColor");
            throw std::runtime_error("Failed to create uniform: u_baseColor");
        }

        u_info = bgfx::createUniform("u_info", bgfx::UniformType::Vec4);

        if (!bgfx::isValid(u_info))
        {
            logger->error("Failed to create uniform: u_info");
            throw std::runtime_error("Failed to create uniform: u_info");
        }

        u_previousModelViewProj = bgfx::createUniform("u_previousModelViewProj", bgfx::UniformType::Mat4);
        if (!bgfx::isValid(u_previousModelViewProj))
        {
            logger->error("Failed to create uniform: u_previousModelViewProj");
            throw std::runtime_error("Failed to create uniform: u_previousModelViewProj");
        }

        u_jitter = bgfx::createUniform("u_jitter", bgfx::UniformType::Vec4);
        if (!bgfx::isValid(u_jitter))
        {
            logger->error("Failed to create uniform: u_jitter");
            throw std::runtime_error("Failed to create uniform: u_jitter");
        }

        logger->info("Field initialized successfully.");
    }
    else
    {
        logger->error("Could not open field config file: {0}, error: {1}", configFile, err.getMessage().getCPtr());
    }

    initGBuffer(window.width, window.height);
    initOIT(window.width, window.height);
    initTAA(window.width, window.height);
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
float previousJitterX = 0.0f;
float previousJitterY = 0.0f;
bool firstFrame = true;

bool firstTAAFrame = true;
bool taaUseBuffer1 = false;
int jitterIndex = 0;

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

void field::render(const blackboard::app::Window &window)
{
    updateOrbitCameraFromInput();
    const bx::Vec3 at = orbitCamera.target;
    const bx::Vec3 eye = getOrbitEye();

    uint16_t m_width = window.width;
    uint16_t m_height = window.height;

    float haltonX = 2.0f * Halton(jitterIndex + 1, 2) - 1.0f;
    float haltonY = 2.0f * Halton(jitterIndex + 1, 3) - 1.0f;
    float jitterX = (haltonX / m_width);
    float jitterY = (haltonY / m_height);

    float view[16];
    bx::mtxLookAt(view, eye, at);

    float proj[16];
    bx::mtxProjInf(proj, 60.0f, float(m_width) / float(m_height), 0.1f, bgfx::getCaps()->homogeneousDepth, bx::Handedness::Left, bx::NearFar::Reverse);
    proj[8] += jitterX;
    proj[9] += jitterY;

    float viewProj[16];
    bx::mtxMul(viewProj, view, proj);

    if (firstFrame)
    {
        std::memcpy(previousViewProj, viewProj, sizeof(viewProj));
        firstFrame = false;
    }

    bgfx::Encoder *encoder = bgfx::begin();

    updateInfo(encoder, 0.1f, 100.0f);

    jitterIndex = (jitterIndex + 1) % HALTON_SAMPLES;

    float jitter[4] = {jitterX, jitterY, previousJitterX, previousJitterY};
    encoder->setUniform(u_jitter, jitter);
    previousJitterX = jitterX;
    previousJitterY = jitterY;

    // OPAQUE PASS

    encoder->discard(BGFX_DISCARD_BINDINGS);

    bgfx::setViewName(VIEW_GBUFFER, "Field - GBuffer");
    bgfx::setViewTransform(VIEW_GBUFFER, view, proj);
    bgfx::setViewRect(VIEW_GBUFFER, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_GBUFFER, gBufFbo);
    bgfx::setViewClear(VIEW_GBUFFER,
                       BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
                       0x00000000,
                       bgfx::getCaps()->homogeneousDepth ? -1.0f : 0.0f);

    float identity[16];
    bx::mtxIdentity(identity);

    for (auto &d : opaqueBatches)
    {
        encoder->setState(
            BGFX_STATE_WRITE_RGB |
            BGFX_STATE_WRITE_A |
            BGFX_STATE_DEPTH_TEST_GREATER |
            BGFX_STATE_WRITE_Z);

        encoder->setTransform(identity);
        encoder->setVertexBuffer(0, d.vbh);
        encoder->setIndexBuffer(d.ibh);

        encoder->setUniform(u_baseColor, d.baseColor);
        encoder->setUniform(u_previousModelViewProj, previousViewProj);

        encoder->submit(VIEW_GBUFFER, program);
    }

    // TRANSPARENT PASS
    bgfx::setViewName(VIEW_OIT, "Field - OIT");

    bgfx::setViewTransform(VIEW_OIT, view, proj);
    bgfx::setViewRect(VIEW_OIT, 0, 0, uint16_t(m_width), uint16_t(m_height));

    bgfx::setViewFrameBuffer(VIEW_OIT, gOitFbo);
    bgfx::setPaletteColor(0, 0, 0, 0, 0); // Clear accum to 0
    bgfx::setPaletteColor(1, 1, 1, 1, 1); // Clear reveal to 1
    bgfx::setViewClear(VIEW_OIT,
                       BGFX_CLEAR_COLOR,
                       0.0f,
                       0,
                       0,
                       1);

    for (auto &d : transparentBatches)
    {
        encoder->setTransform(identity);
        encoder->setVertexBuffer(0, d.vbh);
        encoder->setIndexBuffer(d.ibh);

        encoder->setUniform(u_baseColor, d.baseColor);
        encoder->setUniform(u_previousModelViewProj, previousViewProj);

        encoder->setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_INDEPENDENT |
                BGFX_STATE_DEPTH_TEST_GREATER
                // RT0
                | BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE,
                                        BGFX_STATE_BLEND_ONE),
            // RT1
            BGFX_STATE_BLEND_FUNC_RT_1(BGFX_STATE_BLEND_ZERO,
                                       BGFX_STATE_BLEND_INV_SRC_ALPHA));

        encoder->submit(VIEW_OIT, programOit);
    }

    // Transparent depth post-pass
    bgfx::setViewName(VIEW_OIT_DEPTH_POST_PASS, "Field - OIT Depth Post-Pass");

    bgfx::setViewTransform(VIEW_OIT_DEPTH_POST_PASS, view, proj);
    bgfx::setViewRect(VIEW_OIT_DEPTH_POST_PASS, 0, 0, uint16_t(m_width), uint16_t(m_height));

    bgfx::setViewFrameBuffer(VIEW_OIT_DEPTH_POST_PASS, gOitDepthPostPassFbo);

#if 0 // IF TRANSPARENT MOTION VECTORS AND DEPTH (transparent TAA)
    for (auto &d : transparentBatches)
    {
        encoder->setTransform(identity);
        encoder->setVertexBuffer(0, d.vbh);
        encoder->setIndexBuffer(d.ibh);

        encoder->setState(
            BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A |
            BGFX_STATE_WRITE_Z |
            BGFX_STATE_DEPTH_TEST_GREATER);

        encoder->submit(VIEW_OIT_DEPTH_POST_PASS, programOitDepthPostPass);
    }
#endif

    // Post processing
    bgfx::setViewName(VIEW_POSTPROCESS, "Field - Post Process");
    bgfx::setViewMode(VIEW_POSTPROCESS, bgfx::ViewMode::Sequential);
    bgfx::setViewTransform(VIEW_POSTPROCESS, view, proj);
    bgfx::setViewRect(VIEW_POSTPROCESS, 0, 0, uint16_t(m_width), uint16_t(m_height));
    bgfx::setViewFrameBuffer(VIEW_POSTPROCESS, BGFX_INVALID_HANDLE);

    encoder->setUniform(u_previousModelViewProj, previousViewProj);

    // OIT Composition
    encoder->setImage(0, gAccumTex, 0, bgfx::Access::Read);
    encoder->setImage(1, gRevealTex, 0, bgfx::Access::Read);
    encoder->setImage(2, gbufAlbedo, 0, bgfx::Access::Read);
    encoder->setImage(3, gbufNormal, 0, bgfx::Access::Read);
    encoder->setImage(4, gFXSwap, 0, bgfx::Access::Write);
    encoder->dispatch(VIEW_POSTPROCESS, oitCompProgram, (m_width + 7) / 8, (m_height + 7) / 8);

    // Camera velocity
    encoder->setImage(0, gbufVelocity, 0, bgfx::Access::Write);
    encoder->setImage(1, gbufDepth, 0, bgfx::Access::Read);
    encoder->dispatch(VIEW_POSTPROCESS, cameraVelocityProgram, (m_width + 7) / 8, (m_height + 7) / 8);

    // SSAO

    // Motion blur

    // TAA resolve
    bgfx::TextureHandle taaOutput = taaUseBuffer1 ? gTAABuffer1 : gTAABuffer0;

    if (firstTAAFrame)
    {
        encoder->blit(VIEW_POSTPROCESS, taaOutput, 0, 0, gFXSwap, 0, 0, m_width, m_height);
        firstTAAFrame = false;
    }
    else
    {
        encoder->setImage(0, gbufVelocity, 0, bgfx::Access::Read);
        encoder->setImage(1, gbufDepth, 0, bgfx::Access::Read);
        encoder->setImage(2, gFXSwap, 0, bgfx::Access::Read);
        encoder->setTexture(3, s_taaHistory, taaUseBuffer1 ? gTAABuffer0 : gTAABuffer1);
        encoder->setImage(4, taaOutput, 0, bgfx::Access::Write);
        encoder->dispatch(VIEW_POSTPROCESS, taaResolveProgram, (m_width + 7) / 8, (m_height + 7) / 8);
    }

    // Bloom

    bgfx::setViewFrameBuffer(VIEW_BLIT, BGFX_INVALID_HANDLE);
    bgfx::setViewName(VIEW_BLIT, "Field - Blit");
    bgfx::setViewRect(VIEW_BLIT, 0, 0, uint16_t(m_width), uint16_t(m_height));

    encoder->setTexture(0, s_tex, taaUseBuffer1 ? gTAABuffer1 : gTAABuffer0);
    encoder->setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);

    screenSpaceQuad(!bgfx::getCaps()->originBottomLeft, encoder);

    encoder->submit(VIEW_BLIT, blitProgram);

    bgfx::end(encoder);

    std::memcpy(previousViewProj, viewProj, sizeof(viewProj));
    taaUseBuffer1 = !taaUseBuffer1;
}

void field::cleanup()
{
    for (auto &d : opaqueBatches)
    {
        if (bgfx::isValid(d.vbh))
            bgfx::destroy(d.vbh);
        if (bgfx::isValid(d.ibh))
            bgfx::destroy(d.ibh);
    }

    for (auto &d : transparentBatches)
    {
        if (bgfx::isValid(d.vbh))
            bgfx::destroy(d.vbh);
        if (bgfx::isValid(d.ibh))
            bgfx::destroy(d.ibh);
    }

    opaqueBatches.clear();
    transparentBatches.clear();

    if (bgfx::isValid(u_baseColor))
        bgfx::destroy(u_baseColor);
    if (bgfx::isValid(u_info))
        bgfx::destroy(u_info);
    if (bgfx::isValid(u_previousModelViewProj))
        bgfx::destroy(u_previousModelViewProj);
    if (bgfx::isValid(u_jitter))
        bgfx::destroy(u_jitter);

    if (bgfx::isValid(program))
        bgfx::destroy(program);
    if (bgfx::isValid(programOit))
        bgfx::destroy(programOit);
    if (bgfx::isValid(programOitDepthPostPass))
        bgfx::destroy(programOitDepthPostPass);
    if (bgfx::isValid(oitCompProgram))
        bgfx::destroy(oitCompProgram);
    if (bgfx::isValid(blitProgram))
        bgfx::destroy(blitProgram);
    if (bgfx::isValid(taaResolveProgram))
        bgfx::destroy(taaResolveProgram);
    if (bgfx::isValid(cameraVelocityProgram))
        bgfx::destroy(cameraVelocityProgram);

    if (bgfx::isValid(gAccumTex))
        bgfx::destroy(gAccumTex);
    if (bgfx::isValid(gRevealTex))
        bgfx::destroy(gRevealTex);
    if (bgfx::isValid(gOitFbo))
        bgfx::destroy(gOitFbo);
    if (bgfx::isValid(gOitDepthPostPassFbo))
        bgfx::destroy(gOitDepthPostPassFbo);

    if (bgfx::isValid(gbufAlbedo))
        bgfx::destroy(gbufAlbedo);
    if (bgfx::isValid(gbufNormal))
        bgfx::destroy(gbufNormal);
    if (bgfx::isValid(gbufVelocity))
        bgfx::destroy(gbufVelocity);
    if (bgfx::isValid(gbufDepth))
        bgfx::destroy(gbufDepth);
    if (bgfx::isValid(gBufFbo))
        bgfx::destroy(gBufFbo);

    if (bgfx::isValid(gFXSwap))
        bgfx::destroy(gFXSwap);

    if (bgfx::isValid(gTAABuffer0))
        bgfx::destroy(gTAABuffer0);
    if (bgfx::isValid(gTAABuffer1))
        bgfx::destroy(gTAABuffer1);

    if (bgfx::isValid(s_tex))
        bgfx::destroy(s_tex);
    if (bgfx::isValid(s_taaHistory))
        bgfx::destroy(s_taaHistory);
}