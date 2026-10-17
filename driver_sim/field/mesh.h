#pragma once

#include "../utils.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cstddef>
#include <utility>
#include <vector>

#include <spdlog/fmt/fmt.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

constexpr std::array<uint8_t, 4> MESH_SERIALIZATION_MAGIC = {0x67, 0x31, 0x67, 0x31};
constexpr uint8_t MESH_SERIALIZATION_VERSION = 10; // increment to force reload cache

enum class MaterialType : uint8_t
{
    Opaque,
    Transparent,
};

/**
 * LOD flags. When targeting a certain LOD, only meshes with that level are drawn.
 * Ex. if you want a mesh to be drawn at and below LOD6, set the LOD flag to LOD6 | LOD7.
 */
enum class MeshLOD : uint8_t
{
    // Highest level of detail
    LOD0 = (1U << 0U),
    LOD1 = (1U << 1U),
    LOD2 = (1U << 2U),
    LOD3 = (1U << 3U),
    LOD4 = (1U << 4U),
    LOD5 = (1U << 5U),
    LOD6 = (1U << 6U),
    // Lowest level of detail
    LOD7 = (1U << 7U),

    LOD_ALL = LOD0 | LOD1 | LOD2 | LOD3 | LOD4 | LOD5 | LOD6 | LOD7,

    LOD_MAX_0 = LOD0,
    LOD_MAX_1 = LOD_MAX_0 | LOD1,
    LOD_MAX_2 = LOD_MAX_1 | LOD2,
    LOD_MAX_3 = LOD_MAX_2 | LOD3,
    LOD_MAX_4 = LOD_MAX_3 | LOD4,
    LOD_MAX_5 = LOD_MAX_4 | LOD5,
    LOD_MAX_6 = LOD_MAX_5 | LOD6,
};

UTILS_DEFINE_ENUM_FLAG_OPERATORS(MeshLOD)

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
        if (containsIgnoreCase(nodeName, "carpet"))
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

struct IslandMap
{
    struct Coordinate
    {
        int32_t x;
        int32_t y;
        int32_t z;

        bool operator==(const Coordinate &other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CoordinateHash
    {
        std::size_t operator()(const Coordinate &coord) const
        {
            std::size_t hash = 2166136261u;
            hash_combine(hash, coord.x);
            hash_combine(hash, coord.y);
            hash_combine(hash, coord.z);
            return hash;
        }
    };

    struct Island
    {
        bx::Vec3 boundsMin{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                           std::numeric_limits<float>::max()};
        bx::Vec3 boundsMax{std::numeric_limits<float>::min(), std::numeric_limits<float>::min(),
                           std::numeric_limits<float>::min()};
    };

    float islandSize = 0.1f;
    std::unordered_map<Coordinate, Island, CoordinateHash> islands;

    Coordinate getIslandCoordinate(const bx::Vec3 &position) const
    {
        return Coordinate{static_cast<int32_t>(std::floor(position.x / islandSize)),
                          static_cast<int32_t>(std::floor(position.y / islandSize)),
                          static_cast<int32_t>(std::floor(position.z / islandSize))};
    }

    Island &getOrCreateIsland(const bx::Vec3 &position)
    {
        Coordinate coord = getIslandCoordinate(position);
        auto it = islands.find(coord);
        if (it != islands.end())
        {
            return it->second;
        }
        else
        {
            Island newIsland;
            islands[coord] = newIsland;
            return islands[coord];
        }
    }

    std::optional<Island> getClosestIsland(const bx::Vec3 &position) const
    {
        Coordinate coord = getIslandCoordinate(position);
        auto it = islands.find(coord);
        if (it != islands.end())
        {
            return it->second;
        }
        else if (!islands.empty())
        {
            int32_t closestDistance = std::numeric_limits<int32_t>::max();
            Island closestIsland = islands.begin()->second; // Initialize with the first island

            for (const auto &[islandCoord, island] : islands)
            {
                int32_t dx = islandCoord.x - coord.x;
                int32_t dy = islandCoord.y - coord.y;
                int32_t dz = islandCoord.z - coord.z;

                int32_t distance = dx * dx + dy * dy + dz * dz;

                if (distance < closestDistance)
                {
                    closestDistance = distance;
                    closestIsland = island;
                }
            }
            return closestIsland;
        }
        else
        {
            return std::nullopt;
        }
    }
};

struct Mesh
{
    std::string tag;
    Material material;
    MeshLOD lod = MeshLOD::LOD_ALL;
    IslandMap islands;

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

    inline std::optional<bx::Vec3> getClosestWorldCenter(bx::Vec3 worldPosition,
                                                         float modelMatrix[16],
                                                         float invModelMatrix[16]) const
    {
        bx::Vec3 localPosition = bx::mul(worldPosition, invModelMatrix);

        std::optional<IslandMap::Island> island = islands.getClosestIsland(localPosition);
        if (!island.has_value())
        {
            return std::nullopt;
        }

        bx::Vec3 center =
            bx::mul(bx::add(island.value().boundsMin, island.value().boundsMax), 0.5f);
        return bx::mul(center, modelMatrix);
    }

    /**
     * tags is a map of mesh-name:tag
     * meshes of the same material but different tags will stay separated and won't be merged.
     * this is useful for dynamically removing meshes based on tags.
     * additionally, meshes with same tag but different materials will also stay separated, since
     * they can't be merged anyway.
     *
     * lodOverrides is a map of mesh-name:MeshLOD, which allows overriding the LOD of specific
     * meshes. this is useful for meshes that are small but important (such as RSL or led strips),
     * and should be drawn at higher LODs than their size would normally make them.
     */
    static void fromGltfModel(std::vector<Mesh> &meshesOut, const fastgltf::Asset &asset,
                              const std::unordered_map<std::string, std::string> &tags,
                              const std::unordered_map<std::string, MeshLOD> &lodOverrides);
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