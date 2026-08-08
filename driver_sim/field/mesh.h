#pragma once

#include "../utils.h"
#include <bgfx/bgfx.h>
#include <cstddef>
#include <utility>
#include <vector>

#include <spdlog/fmt/fmt.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#ifndef STR_HELPER
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#endif

constexpr std::array<uint8_t, 4> MESH_SERIALIZATION_MAGIC = {0x67, 0x31, 0x67, 0x31};
constexpr uint8_t MESH_SERIALIZATION_VERSION = 9; // increment to force reload cache

enum class MaterialType : uint8_t
{
    Opaque,
    Transparent
};

namespace fmt
{
    template <> struct formatter<MaterialType> : formatter<string_view>
    {
        template <typename FormatContext> auto format(MaterialType type, FormatContext &ctx) const
        {
            string_view name = "Unknown";
            switch (type)
            {
            case MaterialType::Opaque:
                name = "Opaque";
                break;
            case MaterialType::Transparent:
                name = "Transparent";
                break;
            }
            return formatter<string_view>::format(name, ctx);
        }
    };
} // namespace fmt

struct Material
{
    MaterialType type;
    std::array<float, 4> baseColor;
    std::array<float, 4> emissionColor;
    bool writesObjectMotionVectors;
    float metallic;
    float roughness;
    std::string texture;

    Material()
        : type(MaterialType::Opaque), baseColor{1.0f, 0.0f, 1.0f, 1.0f},
          emissionColor{0.0f, 0.0f, 0.0f, 0.0f}, writesObjectMotionVectors(false), metallic(0.0f),
          roughness(0.5f)
    {
    }

    Material(const fastgltf::Material &mat, const std::string &nodeName = "")
        : baseColor{mat.pbrData.baseColorFactor[0], mat.pbrData.baseColorFactor[1],
                    mat.pbrData.baseColorFactor[2], mat.pbrData.baseColorFactor[3]},
          emissionColor{mat.emissiveFactor[0], mat.emissiveFactor[1], mat.emissiveFactor[2],
                        mat.emissiveStrength},
          type(mat.alphaMode == fastgltf::AlphaMode::Blend ? MaterialType::Transparent
                                                           : MaterialType::Opaque),
          writesObjectMotionVectors(false)
    {
        // cad export pbrData is wrong, base it off the node name instead
        if (nodeName.find("FE-" STR(GAME_YEAR) "-01") != std::string::npos)
        {
            // playing field carpet floor is rough
            metallic = 0.0f;
            roughness = 0.6f;
            baseColor[0] = 0.9f;
            baseColor[1] = 0.9f;
            baseColor[2] = 0.9f;
            baseColor[3] = 1.0f;
            texture = "carpet"; // carpet texture
        }
        else
        {
            metallic = 0.0f;
            roughness = 0.4f;
            texture = "";
        }
    }

    bool operator==(const Material &other) const
    {
        return bit_equal(baseColor[0], other.baseColor[0]) &&
               bit_equal(baseColor[1], other.baseColor[1]) &&
               bit_equal(baseColor[2], other.baseColor[2]) &&
               bit_equal(baseColor[3], other.baseColor[3]) &&
               bit_equal(emissionColor[0], other.emissionColor[0]) &&
               bit_equal(emissionColor[1], other.emissionColor[1]) &&
               bit_equal(emissionColor[2], other.emissionColor[2]) &&
               bit_equal(emissionColor[3], other.emissionColor[3]) &&
               bit_equal(metallic, other.metallic) && bit_equal(roughness, other.roughness) &&
               texture == other.texture && type == other.type;
    }
};

struct MaterialHash
{
    std::size_t operator()(const Material &mat) const
    {
        std::size_t hash = 2166136261u;
        for (float color : mat.baseColor)
        {
            hash_combine(hash, color);
        }

        for (float color : mat.emissionColor)
        {
            hash_combine(hash, color);
        }

        hash_combine(hash, mat.type);
        hash_combine(hash, mat.texture);
        hash_combine(hash, mat.metallic);
        hash_combine(hash, mat.roughness);

        return hash;
    }
};

namespace std
{
    template <> struct hash<Material>
    {
        size_t operator()(const Material &mat) const noexcept { return MaterialHash{}(mat); }
    };
} // namespace std

struct MeshVertex
{
    float x, y, z;
    uint32_t normal;
    float u, v;

    static bgfx::VertexLayout layout;

    static void init()
    {
        layout.begin()
            .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Normal, 4, bgfx::AttribType::Uint8, true, true)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .end();
    }
};

struct Mesh
{
    std::string tag;
    Material material;

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;

    Mesh() = default;
    Mesh(Material mat) : material(std::move(mat)) {}

    bool createBuffers()
    {
        if (!vertices.empty())
        {
            vertexBuffer = bgfx::createVertexBuffer(
                bgfx::makeRef(vertices.data(), vertices.size() * sizeof(MeshVertex)),
                MeshVertex::layout);
        }

        if (!indices.empty())
        {
            indexBuffer = bgfx::createIndexBuffer(
                bgfx::makeRef(indices.data(), indices.size() * sizeof(uint32_t)),
                BGFX_BUFFER_INDEX32);
        }

        return bgfx::isValid(vertexBuffer) && bgfx::isValid(indexBuffer);
    }

    void destroy()
    {
        if (bgfx::isValid(vertexBuffer))
        {
            bgfx::destroy(vertexBuffer);
            vertexBuffer = BGFX_INVALID_HANDLE;
        }

        if (bgfx::isValid(indexBuffer))
        {
            bgfx::destroy(indexBuffer);
            indexBuffer = BGFX_INVALID_HANDLE;
        }

        vertices.clear();
        indices.clear();
    }

    /**
     * tags is a map of mesh-name:tag
     * meshes of the same material but different tags will stay separated and won't be merged.
     * this is useful for dynamically removing meshes based on tags.
     * additionally, meshes with same tag but different materials will also stay separated, since
     * they can't be merged anyway.
     */
    static void fromGltfModel(std::vector<Mesh> &meshesOut, const fastgltf::Asset &asset,
                              const std::unordered_map<std::string, std::string> &tags);
    static void fromSerialized(std::vector<Mesh> &meshesOut, const std::filesystem::path &path);
    static void toSerialized(const std::vector<Mesh> &meshes, const std::filesystem::path &path);
    static void createBuffersForMeshes(std::vector<Mesh> &meshes);
    static void createBuffersForMeshes(Mesh &mesh);

    /**
     * Adds a cube mesh to the specified mesh.
     * @param mesh The mesh to add the cube to.
     * @param centerX The x-coordinate of the cube's center.
     * @param centerY The y-coordinate of the cube's center.
     * @param centerZ The z-coordinate of the cube's center.
     * @param width The width of the cube.
     * @param height The height of the cube.
     * @param depth The depth of the cube.
     * @param projectedUVs Whether to use projected UV coordinates from the +x face (useful if world
     * space orientation of the uvs matters).
     */
    static void addCube(Mesh &mesh, float centerX, float centerY, float centerZ, float width,
                        float height, float depth, bool projectedUVs = false);

    static Material *getTaggedMaterial(std::vector<Mesh> &meshes, const std::string_view &tag)
    {
        for (auto &mesh : meshes)
        {
            if (mesh.tag == tag)
            {
                return &mesh.material;
            }
        }
        return nullptr;
    }
};