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
                    mat = Material(asset.materials[primitive.materialIndex.value()], nodeName);
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
                        v.u = 0.0f;
                        v.v = 0.0f;
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

                auto texcoordIt = primitive.findAttribute("TEXCOORD_0");
                if (texcoordIt != primitive.attributes.end())
                {
                    auto &texcoordAccessor = asset.accessors[texcoordIt->accessorIndex];
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset,
                        texcoordAccessor,
                        [&](fastgltf::math::fvec2 texcoord, std::size_t index)
                        {
                            mesh.vertices[baseVertex + index].u = texcoord.x();
                            mesh.vertices[baseVertex + index].v = texcoord.y();
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
        Mesh::createBuffersForMeshes(mesh);
    }
}

void Mesh::createBuffersForMeshes(Mesh &mesh)
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

void Mesh::toSerialized(const std::vector<Mesh> &meshes, const std::filesystem::path &path)
{
    std::ofstream outFile(path, std::ios::binary);
    if (!outFile)
    {
        logger->error("Failed to open file for writing: {}", path.string());
        throw std::runtime_error("Failed to open file for writing.");
    }

    // write magic
    outFile.write(reinterpret_cast<const char *>(MESH_SERIALIZATION_MAGIC), sizeof(MESH_SERIALIZATION_MAGIC));

    // write version
    outFile.write(reinterpret_cast<const char *>(&MESH_SERIALIZATION_VERSION), sizeof(MESH_SERIALIZATION_VERSION));

    uint32_t meshCount = static_cast<uint32_t>(meshes.size());
    outFile.write(reinterpret_cast<const char *>(&meshCount), sizeof(meshCount));

    for (const auto &mesh : meshes)
    {
        // write tag
        uint32_t tagLength = static_cast<uint32_t>(mesh.tag.length());
        outFile.write(reinterpret_cast<const char *>(&tagLength), sizeof(tagLength));
        outFile.write(reinterpret_cast<const char *>(mesh.tag.data()), tagLength);

        // write material
        outFile.write(reinterpret_cast<const char *>(&mesh.material.type), sizeof(mesh.material.type));
        outFile.write(reinterpret_cast<const char *>(mesh.material.baseColor.data()), mesh.material.baseColor.size() * sizeof(float));
        outFile.write(reinterpret_cast<const char *>(mesh.material.emissionColor.data()), mesh.material.emissionColor.size() * sizeof(float));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.writesObjectMotionVectors), sizeof(mesh.material.writesObjectMotionVectors));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.metallic), sizeof(mesh.material.metallic));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.roughness), sizeof(mesh.material.roughness));
        uint32_t textureLength = static_cast<uint32_t>(mesh.material.texture.length());
        outFile.write(reinterpret_cast<const char *>(&textureLength), sizeof(textureLength));
        outFile.write(reinterpret_cast<const char *>(mesh.material.texture.data()), textureLength);
        
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

    uint8_t magic[sizeof(MESH_SERIALIZATION_MAGIC)];
    inFile.read(reinterpret_cast<char *>(magic), sizeof(magic));
    if (memcmp(magic, MESH_SERIALIZATION_MAGIC, sizeof(MESH_SERIALIZATION_MAGIC)) != 0)
    {
        logger->error("Invalid mesh magic.");
        throw std::runtime_error("Invalid mesh magic.");
    }

    uint8_t serializationVersion;
    inFile.read(reinterpret_cast<char *>(&serializationVersion), sizeof(serializationVersion));
    if (serializationVersion != MESH_SERIALIZATION_VERSION)
    {
        logger->error("Invalid mesh serialization version.");
        throw std::runtime_error("Invalid mesh serialization version.");
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

        // read material
        Material material;
        inFile.read(reinterpret_cast<char *>(&material.type), sizeof(material.type));
        inFile.read(reinterpret_cast<char *>(material.baseColor.data()), material.baseColor.size() * sizeof(float));
        inFile.read(reinterpret_cast<char *>(material.emissionColor.data()), material.emissionColor.size() * sizeof(float));
        inFile.read(reinterpret_cast<char *>(&material.writesObjectMotionVectors), sizeof(material.writesObjectMotionVectors));
        inFile.read(reinterpret_cast<char *>(&material.metallic), sizeof(material.metallic));
        inFile.read(reinterpret_cast<char *>(&material.roughness), sizeof(material.roughness));
        uint32_t textureLength;
        inFile.read(reinterpret_cast<char *>(&textureLength), sizeof(textureLength));
        if (textureLength > 0)
        {
            material.texture.resize(textureLength);
            inFile.read(reinterpret_cast<char *>(material.texture.data()), textureLength);
        }
        mesh.material = material;

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

void Mesh::addCube(Mesh& mesh,
                   float cx, float cy, float cz,
                   float w, float h, float d,
                    bool projectedUVs)
{
    const float hx = w * 0.5f;
    const float hy = h * 0.5f;
    const float hz = d * 0.5f;

    struct Face {
        fastgltf::math::fvec3 n;
        fastgltf::math::fvec3 v0, v1, v2, v3;
    };

    Face faces[6] = {
        // -X
        {{-1,0,0}, {-hx,-hy,-hz}, {-hx,-hy,hz}, {-hx,hy,hz}, {-hx,hy,-hz}},
        // +X
        {{ 1,0,0}, { hx,-hy,hz}, { hx,-hy,-hz}, { hx,hy,-hz}, { hx,hy,hz}},
        // -Y
        {{0,-1,0}, {-hx,-hy,-hz}, { hx,-hy,-hz}, { hx,-hy,hz}, {-hx,-hy,hz}},
        // +Y
        {{0, 1,0}, {-hx,hy,hz}, { hx,hy,hz}, { hx,hy,-hz}, {-hx,hy,-hz}},
        // -Z
        {{0,0,-1}, { hx,-hy,-hz}, {-hx,-hy,-hz}, {-hx,hy,-hz}, { hx,hy,-hz}},
        // +Z
        {{0,0, 1}, {-hx,-hy,hz}, { hx,-hy,hz}, { hx,hy,hz}, {-hx,hy,hz}},
    };

    uint32_t startIndex = (uint32_t)mesh.vertices.size();

    mesh.vertices.reserve(mesh.vertices.size() + 24);
    mesh.indices.reserve(mesh.indices.size() + 36);

    for (int f = 0; f < 6; ++f)
    {
        auto& face = faces[f];

        const fastgltf::math::fvec3 corners[4] = {
            face.v0, face.v1, face.v2, face.v3
        };

        MeshVertex v[4];

        for (int i = 0; i < 4; ++i)
        {
            v[i].x = cx + corners[i].x();
            v[i].y = cy + corners[i].y();
            v[i].z = cz + corners[i].z();

            v[i].normal = encodeNormalRgba8(face.n, 0.0f);

            if(projectedUVs)
            {
                // Projected UVs from +X face
                v[i].u = corners[i].y() / h + 0.5f;
                v[i].v = -corners[i].z() / d + 0.5f;
            }
            else
            {
                v[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
                v[i].v = (i >= 2) ? 1.0f : 0.0f;
            }

            mesh.vertices.push_back(v[i]);
        }

        uint32_t base = startIndex + f * 4;

        // two triangles per face
        mesh.indices.insert(mesh.indices.end(), {
            base + 0, base + 1, base + 2,
            base + 0, base + 2, base + 3
        });
    }
}