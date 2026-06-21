#include "mesh.h"

#include <blackboard_app/logger.h>
#include <bx/error.h>
#include <bx/pixelformat.h>

using namespace blackboard::logger;

bgfx::VertexLayout MeshVertex::layout;

/// Encode normal vector as RGBA8 color value.
///
/// @param[in] _x X component of the normal.
/// @param[in] _y Y component of the normal.
/// @param[in] _z Z component of the normal.
/// @param[in] _w W component.
///
/// @returns Packed RGBA8 value.
///
inline uint32_t encodeNormalRgba8(fastgltf::math::fvec3 normal, float w)
{
    const float src[] =
        {
            normal.x() * 0.5f + 0.5f,
            normal.y() * 0.5f + 0.5f,
            normal.z() * 0.5f + 0.5f,
            w * 0.5f + 0.5f,
        };
    uint32_t dst;
    bx::packRgba8(&dst, src);
    return dst;
}

struct MeshGroupKey
{
    Material material;
    std::string tag;

    bool operator==(const MeshGroupKey &other) const
    {
        return material == other.material &&
               tag == other.tag;
    }
};

struct MeshGroupKeyHash
{
    std::size_t operator()(const MeshGroupKey &key) const
    {
        std::size_t h = 2166136261u;

        hash_combine(h, key.material);
        hash_combine(h, key.tag);

        return h;
    }
};

void Mesh::fromGltfModel(std::vector<Mesh> &meshesOut, const fastgltf::Asset &asset, const std::unordered_map<std::string, std::string> &tags)
{
    const std::size_t sceneIndex = asset.defaultScene.value_or(0);

    std::unordered_map<MeshGroupKey, size_t, MeshGroupKeyHash> meshMap;
    std::unordered_map<std::string, int> duplicateNodeResolver;

    fastgltf::iterateSceneNodes(asset, sceneIndex, fastgltf::math::fmat4x4(), [&](const fastgltf::Node &node, const fastgltf::math::fmat4x4 &worldMatrix)
                                {
            if (!node.meshIndex.has_value() || node.meshIndex.value() >= asset.meshes.size())
            {
                return;
            }
            
            fastgltf::math::fmat3x3 normalMatrix = fastgltf::math::transpose(fastgltf::math::inverse(fastgltf::math::fmat3x3(worldMatrix)));

            auto &gltfMesh = asset.meshes[node.meshIndex.value()];

            std::string nodeName = std::string(node.name);
            nodeName.erase(std::remove(nodeName.begin(), nodeName.end(), ':'), nodeName.end());
            std::replace(nodeName.begin(), nodeName.end(), ' ', '_');
            
            if(duplicateNodeResolver.find(nodeName) != duplicateNodeResolver.end())
            {
                int count = duplicateNodeResolver[nodeName]++;
                nodeName += "_" + std::to_string(count);
            }
            else
            {
                duplicateNodeResolver[nodeName] = 1;
            }

            auto tagIt = tags.find(nodeName);
            const std::string meshTag =
                (tagIt != tags.end()) ? tagIt->second : "";

            for (auto &primitive : gltfMesh.primitives)
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
                auto &positionAccessor = asset.accessors[positionIt->accessorIndex];
                auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];

                if (positionAccessor.count == 0 || indicesAccessor.count == 0)
                {
                    logger->warn("Skipping primitive with empty geometry data.");
                    continue;
                }

                Material mat;
                if (primitive.materialIndex.has_value() && primitive.materialIndex.value() < asset.materials.size())
                {
                    mat = Material(asset.materials[primitive.materialIndex.value()]);
                }

                MeshGroupKey key{
                    .material = mat,
                    .tag = meshTag
                };

                auto it = meshMap.find(key);
                Mesh* meshPtr = nullptr;

                if (it == meshMap.end())
                {
                    meshMap[key] = meshesOut.size();

                    auto& mesh = meshesOut.emplace_back(mat);
                    mesh.tag = meshTag;

                    meshPtr = &mesh;
                }
                else
                {
                    meshPtr = &meshesOut[it->second];
                }

                Mesh& mesh = *meshPtr;

                const uint32_t baseVertex = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.reserve(mesh.vertices.size() + positionAccessor.count);
                mesh.indices.reserve(mesh.indices.size() + indicesAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset,
                    positionAccessor,
                    [&](fastgltf::math::fvec3 pos, std::size_t index)
                    {
                        fastgltf::math::fvec4 worldPos = worldMatrix * fastgltf::math::fvec4(pos.x(), pos.y(), pos.z(), 1.0f);
                        MeshVertex v{};
                        v.x = worldPos.x();
                        v.y = worldPos.y();
                        v.z = worldPos.z();
                        v.normal = 0;
                        mesh.vertices.push_back(v);
                    });

                auto normalIt = primitive.findAttribute("NORMAL");
                if (normalIt != primitive.attributes.end())
                {
                    auto &normalAccessor = asset.accessors[normalIt->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset,
                        normalAccessor,
                        [&](fastgltf::math::fvec3 norm, std::size_t index)
                        {
                            fastgltf::math::fvec3 worldNorm = normalMatrix * norm;
                            mesh.vertices[baseVertex + index].normal = encodeNormalRgba8(worldNorm, 0.0f);
                        });
                }
                    
                fastgltf::iterateAccessor<uint32_t>(
                    asset,
                    indicesAccessor,
                    [&](uint32_t idx)
                    {
                        mesh.indices.push_back(baseVertex + idx);
                    });
            } });
}

void Mesh::createBuffersForMeshes(std::vector<Mesh> &meshes)
{
    for (auto &mesh : meshes)
    {
        if (!mesh.createBuffers())
        {
            logger->error("Failed to create buffers for mesh with material: type={}, baseColor=({}, {}, {}, {})",
                          mesh.material.type,
                          mesh.material.baseColor[0],
                          mesh.material.baseColor[1],
                          mesh.material.baseColor[2],
                          mesh.material.baseColor[3]);
            throw std::runtime_error("Failed to create buffers for mesh.");
        }
    }
}

void Mesh::toSerialized(const std::vector<Mesh> &meshes, const std::filesystem::path &path)
{
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile)
    {
        logger->error("Failed to open file for writing: {}", path.string());
        throw std::runtime_error("Failed to open file for writing.");
    }

    uint32_t meshCount = static_cast<uint32_t>(meshes.size());
    outFile.write(reinterpret_cast<const char *>(&meshCount), sizeof(meshCount));

    for (const auto &mesh : meshes)
    {
        // write tag
        uint32_t tagLength = static_cast<uint32_t>(mesh.tag.length());
        outFile.write(reinterpret_cast<const char *>(&tagLength), sizeof(tagLength));
        outFile.write(reinterpret_cast<const char *>(mesh.tag.data()), tagLength);

        outFile.write(reinterpret_cast<const char *>(&mesh.material), sizeof(mesh.material));

        uint32_t vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        outFile.write(reinterpret_cast<const char *>(&vertexCount), sizeof(vertexCount));
        outFile.write(reinterpret_cast<const char *>(mesh.vertices.data()), vertexCount * sizeof(MeshVertex));

        uint32_t indexCount = static_cast<uint32_t>(mesh.indices.size());
        outFile.write(reinterpret_cast<const char *>(&indexCount), sizeof(indexCount));
        outFile.write(reinterpret_cast<const char *>(mesh.indices.data()), indexCount * sizeof(uint32_t));
    }
}

void Mesh::fromSerialized(std::vector<Mesh> &meshesOut, const std::filesystem::path &path)
{
    std::ifstream inFile(path, std::ios::binary);
    if (!inFile)
    {
        logger->error("Failed to open file for reading: {}", path.string());
        throw std::runtime_error("Failed to open file for reading.");
    }

    uint32_t meshCount;
    inFile.read(reinterpret_cast<char *>(&meshCount), sizeof(meshCount));

    for (uint32_t i = 0; i < meshCount; ++i)
    {
        Mesh mesh;

        // read tag
        uint32_t tagLength;
        inFile.read(reinterpret_cast<char *>(&tagLength), sizeof(tagLength));
        if (tagLength > 0)
        {
            std::string tag(tagLength, '\0');
            inFile.read(reinterpret_cast<char *>(tag.data()), tagLength);
            mesh.tag = tag;
        }

        inFile.read(reinterpret_cast<char *>(&mesh.material), sizeof(mesh.material));

        uint32_t vertexCount;
        inFile.read(reinterpret_cast<char *>(&vertexCount), sizeof(vertexCount));
        mesh.vertices.resize(vertexCount);
        inFile.read(reinterpret_cast<char *>(mesh.vertices.data()), vertexCount * sizeof(MeshVertex));

        uint32_t indexCount;
        inFile.read(reinterpret_cast<char *>(&indexCount), sizeof(indexCount));
        mesh.indices.resize(indexCount);
        inFile.read(reinterpret_cast<char *>(mesh.indices.data()), indexCount * sizeof(uint32_t));

        meshesOut.push_back(std::move(mesh));
    }
}