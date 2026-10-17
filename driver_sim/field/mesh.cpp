#include "mesh.h"

#include "../gziputils.h"
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
/// @param[in] normalW W component.
///
/// @returns Packed RGBA8 value.
///
inline uint32_t encodeNormalRgba8(fastgltf::math::fvec3 normal, float normalW)
{
    const float src[] = {
        (normal.x() * 0.5f) + 0.5f,
        (normal.y() * 0.5f) + 0.5f,
        (normal.z() * 0.5f) + 0.5f,
        (normalW * 0.5f) + 0.5f,
    };
    uint32_t dst{};
    bx::packRgba8(&dst, src);
    return dst;
}

struct MeshGroupKey
{
    Material material;
    std::string tag;
    MeshLOD lod;

    bool operator==(const MeshGroupKey &other) const
    {
        return material == other.material && tag == other.tag && lod == other.lod;
    }
};

struct MeshGroupKeyHash
{
    std::size_t operator()(const MeshGroupKey &key) const
    {
        std::size_t hash = 2166136261u;

        hash_combine(hash, key.material);
        hash_combine(hash, key.tag);
        hash_combine(hash, static_cast<std::underlying_type<MeshLOD>::type>(key.lod));

        return hash;
    }
};

void Mesh::fromGltfModel(std::vector<Mesh> &meshesOut, const fastgltf::Asset &asset,
                         const std::unordered_map<std::string, std::string> &tags,
                         const std::unordered_map<std::string, MeshLOD> &lodOverrides)
{
    const std::size_t sceneIndex = asset.defaultScene.value_or(0);

    std::unordered_map<MeshGroupKey, size_t, MeshGroupKeyHash> meshMap;
    std::unordered_map<std::string, int> duplicateNodeResolver;

    fastgltf::iterateSceneNodes(
        asset, sceneIndex, fastgltf::math::fmat4x4(),
        [&](const fastgltf::Node &node, const fastgltf::math::fmat4x4 &worldMatrix)
        {
            if (!node.meshIndex.has_value() || node.meshIndex.value() >= asset.meshes.size())
            {
                return;
            }

            fastgltf::math::fmat3x3 normalMatrix = fastgltf::math::transpose(
                fastgltf::math::inverse(fastgltf::math::fmat3x3(worldMatrix)));

            const auto &gltfMesh = asset.meshes.at(node.meshIndex.value());

            std::string nodeName = std::string(node.name);
            std::erase(nodeName, ':');
            std::ranges::replace(nodeName, ' ', '_');

            if (duplicateNodeResolver.contains(nodeName))
            {
                int count = duplicateNodeResolver[nodeName]++;
                nodeName += "_" + std::to_string(count);
            }
            else
            {
                duplicateNodeResolver[nodeName] = 1;
            }

            auto tagIt = tags.find(nodeName);
            const std::string meshTag = (tagIt != tags.end()) ? tagIt->second : "";

            auto lodIt = lodOverrides.find(nodeName);
            std::optional<MeshLOD> lodOverride =
                (lodIt != lodOverrides.end()) ? std::make_optional(lodIt->second) : std::nullopt;

            for (const auto &primitive : gltfMesh.primitives)
            {
                const auto *positionIt = primitive.findAttribute("POSITION");
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
                const auto &positionAccessor = asset.accessors.at(positionIt->accessorIndex);
                const auto &indicesAccessor = asset.accessors.at(primitive.indicesAccessor.value());

                if (positionAccessor.count == 0 || indicesAccessor.count == 0)
                {
                    logger->warn("Skipping primitive with empty geometry data.");
                    continue;
                }

                MeshLOD lod = lodOverride.value_or(MeshLOD::LOD_ALL);

                if (!lodOverride.has_value() && positionAccessor.min.has_value() &&
                    positionAccessor.max.has_value())
                {
                    const auto &min = positionAccessor.min.value();
                    const auto &max = positionAccessor.max.value();

                    fastgltf::math::fvec3 minVec;
                    fastgltf::math::fvec3 maxVec;

                    for (size_t i = 0; i < 3; ++i)
                    {
                        if (min.type() == fastgltf::AccessorBoundsArray::BoundsType::float64)
                        {
                            minVec[i] = static_cast<float>(min.get<double>(i));
                            maxVec[i] = static_cast<float>(max.get<double>(i));
                        }
                        else
                        {
                            minVec[i] = static_cast<float>(min.get<int64_t>(i));
                            maxVec[i] = static_cast<float>(max.get<int64_t>(i));
                        }
                    }

                    fastgltf::math::fvec3 size = maxVec - minVec;

                    // Prevent super high aspect ratio from having small volume
                    float minSize = std::min(size.x(), std::min(size.y(), size.z()));
                    float maxSize = std::max(size.x(), std::max(size.y(), size.z()));
                    float aspectRatio = std::clamp(minSize / maxSize, 0.0f, 1.0f);
                    aspectRatio *=
                        aspectRatio; // square the aspect ratio to make it more aggressive
                    size = fastgltf::math::fvec3(
                        size.x() * aspectRatio + maxSize * (1.0f - aspectRatio),
                        size.y() * aspectRatio + maxSize * (1.0f - aspectRatio),
                        size.z() * aspectRatio + maxSize * (1.0f - aspectRatio));

                    float volume = size.x() * size.y() * size.z();

                    // Add vertex count to LOD
                    uint32_t vertexCount = static_cast<uint32_t>(positionAccessor.count);
                    volume /= static_cast<float>(vertexCount / 10000.0f);

                    if (volume < 0.001f)
                    {
                        lod = MeshLOD::LOD_MAX_0;
                    }
                    else if (volume < 0.002f)
                    {
                        lod = MeshLOD::LOD_MAX_1;
                    }
                    else if (volume < 0.005f)
                    {
                        lod = MeshLOD::LOD_MAX_2;
                    }
                    else if (volume < 0.008f)
                    {
                        lod = MeshLOD::LOD_MAX_3;
                    }
                    else if (volume < 0.01f)
                    {
                        lod = MeshLOD::LOD_MAX_4;
                    }
                    else if (volume < 0.015f)
                    {
                        lod = MeshLOD::LOD_MAX_5;
                    }
                    else if (volume < 0.02f)
                    {
                        lod = MeshLOD::LOD_MAX_6;
                    }
                }

                Material mat;
                if (primitive.materialIndex.has_value() &&
                    primitive.materialIndex.value() < asset.materials.size())
                {
                    mat = Material(asset.materials.at(primitive.materialIndex.value()), nodeName);
                }

                MeshGroupKey key{.material = mat, .tag = meshTag, .lod = lod};

                auto itr = meshMap.find(key);
                Mesh *meshPtr = nullptr;

                if (itr == meshMap.end())
                {
                    meshMap[key] = meshesOut.size();

                    auto &mesh = meshesOut.emplace_back(mat);
                    mesh.tag = meshTag;
                    mesh.lod = lod;

                    meshPtr = &mesh;
                }
                else
                {
                    meshPtr = &meshesOut.at(itr->second);
                }

                Mesh &mesh = *meshPtr;

                const auto baseVertex = static_cast<uint32_t>(mesh.vertices.size());
                mesh.vertices.reserve(mesh.vertices.size() + positionAccessor.count);
                mesh.indices.reserve(mesh.indices.size() + indicesAccessor.count);

                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    asset, positionAccessor,
                    [&](fastgltf::math::fvec3 pos, std::size_t index)
                    {
                        fastgltf::math::fvec4 worldPos =
                            worldMatrix * fastgltf::math::fvec4(pos.x(), pos.y(), pos.z(), 1.0f);
                        MeshVertex vertex{};
                        vertex.x = worldPos.x();
                        vertex.y = worldPos.y();
                        vertex.z = worldPos.z();
                        vertex.normal = 0;
                        vertex.u = 0.0f;
                        vertex.v = 0.0f;
                        mesh.vertices.push_back(vertex);

                        IslandMap::Island &island =
                            mesh.islands.getOrCreateIsland(bx::Vec3(vertex.x, vertex.y, vertex.z));
                        island.boundsMin.x = std::min(island.boundsMin.x, vertex.x);
                        island.boundsMin.y = std::min(island.boundsMin.y, vertex.y);
                        island.boundsMin.z = std::min(island.boundsMin.z, vertex.z);
                        island.boundsMax.x = std::max(island.boundsMax.x, vertex.x);
                        island.boundsMax.y = std::max(island.boundsMax.y, vertex.y);
                        island.boundsMax.z = std::max(island.boundsMax.z, vertex.z);
                    });

                const auto *normalIt = primitive.findAttribute("NORMAL");
                if (normalIt != primitive.attributes.end())
                {
                    const auto &normalAccessor = asset.accessors.at(normalIt->accessorIndex);
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                        asset, normalAccessor,
                        [&](fastgltf::math::fvec3 norm, std::size_t index)
                        {
                            fastgltf::math::fvec3 worldNorm = normalMatrix * norm;
                            mesh.vertices.at(baseVertex + index).normal =
                                encodeNormalRgba8(worldNorm, 0.0f);
                        });
                }

                const auto *texcoordIt = primitive.findAttribute("TEXCOORD_0");
                if (texcoordIt != primitive.attributes.end())
                {
                    const auto &texcoordAccessor = asset.accessors.at(texcoordIt->accessorIndex);
                    fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                        asset, texcoordAccessor,
                        [&](fastgltf::math::fvec2 texcoord, std::size_t index)
                        {
                            mesh.vertices.at(baseVertex + index).u = texcoord.x();
                            mesh.vertices.at(baseVertex + index).v = texcoord.y();
                        });
                }

                fastgltf::iterateAccessor<uint32_t>(asset, indicesAccessor, [&](uint32_t idx)
                                                    { mesh.indices.push_back(baseVertex + idx); });
            }
        });
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
        logger->error(
            "Failed to create buffers for mesh with material: type={}, baseColor=({}, {}, {}, {})",
            mesh.material.type, mesh.material.baseColor[0], mesh.material.baseColor[1],
            mesh.material.baseColor[2], mesh.material.baseColor[3]);
        throw std::runtime_error("Failed to create buffers for mesh.");
    }
}

void Mesh::toSerialized(const std::vector<Mesh> &meshes, const std::filesystem::path &path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        logger->error("Failed to open file for writing: {}", path.string());
        throw std::runtime_error("Failed to open file for writing.");
    }

    GzipOutputBuf gzipBuffer(file);
    std::ostream outFile(&gzipBuffer);

    // write magic
    outFile.write(reinterpret_cast<const char *>(MESH_SERIALIZATION_MAGIC.data()),
                  MESH_SERIALIZATION_MAGIC.size());

    // write version
    outFile.write(reinterpret_cast<const char *>(&MESH_SERIALIZATION_VERSION),
                  sizeof(MESH_SERIALIZATION_VERSION));

    // write meshes
    auto meshCount = static_cast<uint32_t>(meshes.size());
    outFile.write(reinterpret_cast<const char *>(&meshCount), sizeof(meshCount));

    for (const auto &mesh : meshes)
    {
        // write tag
        auto tagLength = static_cast<uint32_t>(mesh.tag.length());
        outFile.write(reinterpret_cast<const char *>(&tagLength), sizeof(tagLength));
        outFile.write(reinterpret_cast<const char *>(mesh.tag.data()), tagLength);

        // write material
        outFile.write(reinterpret_cast<const char *>(&mesh.material.type),
                      sizeof(mesh.material.type));
        outFile.write(reinterpret_cast<const char *>(mesh.material.baseColor.data()),
                      static_cast<std::streamsize>(mesh.material.baseColor.size() * sizeof(float)));
        outFile.write(
            reinterpret_cast<const char *>(mesh.material.emissionColor.data()),
            static_cast<std::streamsize>(mesh.material.emissionColor.size() * sizeof(float)));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.writesObjectMotionVectors),
                      sizeof(mesh.material.writesObjectMotionVectors));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.metallic),
                      sizeof(mesh.material.metallic));
        outFile.write(reinterpret_cast<const char *>(&mesh.material.roughness),
                      sizeof(mesh.material.roughness));
        auto textureLength = static_cast<uint32_t>(mesh.material.texture.length());
        outFile.write(reinterpret_cast<const char *>(&textureLength), sizeof(textureLength));
        outFile.write(reinterpret_cast<const char *>(mesh.material.texture.data()), textureLength);

        // write lod
        outFile.write(reinterpret_cast<const char *>(&mesh.lod), sizeof(mesh.lod));

        // write islands
        auto islandCount = static_cast<uint32_t>(mesh.islands.islands.size());
        outFile.write(reinterpret_cast<const char *>(&islandCount), sizeof(islandCount));
        for (const auto &[coordinate, island] : mesh.islands.islands)
        {
            outFile.write(reinterpret_cast<const char *>(&coordinate.x), sizeof(coordinate.x));
            outFile.write(reinterpret_cast<const char *>(&coordinate.y), sizeof(coordinate.y));
            outFile.write(reinterpret_cast<const char *>(&coordinate.z), sizeof(coordinate.z));
            outFile.write(reinterpret_cast<const char *>(&island.boundsMin),
                          sizeof(island.boundsMin));
            outFile.write(reinterpret_cast<const char *>(&island.boundsMax),
                          sizeof(island.boundsMax));
        }

        // write vertices
        auto vertexCount = static_cast<uint32_t>(mesh.vertices.size());
        outFile.write(reinterpret_cast<const char *>(&vertexCount), sizeof(vertexCount));
        outFile.write(reinterpret_cast<const char *>(mesh.vertices.data()),
                      static_cast<std::streamsize>(vertexCount * sizeof(MeshVertex)));

        // write indices
        auto indexCount = static_cast<uint32_t>(mesh.indices.size());
        outFile.write(reinterpret_cast<const char *>(&indexCount), sizeof(indexCount));
        outFile.write(reinterpret_cast<const char *>(mesh.indices.data()),
                      static_cast<std::streamsize>(indexCount * sizeof(uint32_t)));
    }

    outFile.flush();
    if (!outFile)
    {
        logger->error("Failed to serialize mesh to file: {}", path.string());
        throw std::runtime_error("Failed to serialize mesh");
    }

    gzipBuffer.finish();
    file.flush();

    if (!file)
    {
        logger->error("Failed to write gzip file: {}", path.string());
        throw std::runtime_error("Failed to write gzip file");
    }
}

void Mesh::fromSerialized(std::vector<Mesh> &meshesOut, const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        logger->error("Failed to open file for reading: {}", path.string());
        throw std::runtime_error("Failed to open file for reading.");
    }

    GzipInputBuf gzipBuffer(file);
    std::istream inFile(&gzipBuffer);

    // read magic
    uint8_t magic[MESH_SERIALIZATION_MAGIC.size()];
    inFile.read(reinterpret_cast<char *>(magic), sizeof(magic));
    if (memcmp(magic, MESH_SERIALIZATION_MAGIC.data(), MESH_SERIALIZATION_MAGIC.size()) != 0)
    {
        logger->error("Invalid mesh magic.");
        throw std::runtime_error("Invalid mesh magic.");
    }

    // read version
    uint8_t serializationVersion{};
    inFile.read(reinterpret_cast<char *>(&serializationVersion), sizeof(serializationVersion));
    if (serializationVersion != MESH_SERIALIZATION_VERSION)
    {
        logger->error("Invalid mesh serialization version.");
        throw std::runtime_error("Invalid mesh serialization version.");
    }

    // read meshes
    uint32_t meshCount{};
    inFile.read(reinterpret_cast<char *>(&meshCount), sizeof(meshCount));

    for (uint32_t i = 0; i < meshCount; ++i)
    {
        Mesh mesh;

        // read tag
        uint32_t tagLength{};
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
        inFile.read(reinterpret_cast<char *>(material.baseColor.data()),
                    material.baseColor.size() * sizeof(float));
        inFile.read(reinterpret_cast<char *>(material.emissionColor.data()),
                    material.emissionColor.size() * sizeof(float));
        inFile.read(reinterpret_cast<char *>(&material.writesObjectMotionVectors),
                    sizeof(material.writesObjectMotionVectors));
        inFile.read(reinterpret_cast<char *>(&material.metallic), sizeof(material.metallic));
        inFile.read(reinterpret_cast<char *>(&material.roughness), sizeof(material.roughness));
        uint32_t textureLength{};
        inFile.read(reinterpret_cast<char *>(&textureLength), sizeof(textureLength));
        if (textureLength > 0)
        {
            material.texture.resize(textureLength);
            inFile.read(reinterpret_cast<char *>(material.texture.data()), textureLength);
        }
        mesh.material = material;

        // read lod
        inFile.read(reinterpret_cast<char *>(&mesh.lod), sizeof(mesh.lod));

        // read islands
        uint32_t islandCount{};
        inFile.read(reinterpret_cast<char *>(&islandCount), sizeof(islandCount));
        for (uint32_t j = 0; j < islandCount; ++j)
        {
            IslandMap::Coordinate coordinate;
            IslandMap::Island island;
            inFile.read(reinterpret_cast<char *>(&coordinate.x), sizeof(coordinate.x));
            inFile.read(reinterpret_cast<char *>(&coordinate.y), sizeof(coordinate.y));
            inFile.read(reinterpret_cast<char *>(&coordinate.z), sizeof(coordinate.z));
            inFile.read(reinterpret_cast<char *>(&island.boundsMin), sizeof(island.boundsMin));
            inFile.read(reinterpret_cast<char *>(&island.boundsMax), sizeof(island.boundsMax));
            mesh.islands.islands[coordinate] = island;
        }

        // read vertices
        uint32_t vertexCount{};
        inFile.read(reinterpret_cast<char *>(&vertexCount), sizeof(vertexCount));
        mesh.vertices.resize(vertexCount);
        inFile.read(reinterpret_cast<char *>(mesh.vertices.data()),
                    static_cast<std::streamsize>(vertexCount * sizeof(MeshVertex)));

        // read indices
        uint32_t indexCount{};
        inFile.read(reinterpret_cast<char *>(&indexCount), sizeof(indexCount));
        mesh.indices.resize(indexCount);
        inFile.read(reinterpret_cast<char *>(mesh.indices.data()),
                    static_cast<std::streamsize>(indexCount * sizeof(uint32_t)));

        meshesOut.push_back(std::move(mesh));
    }
}

void Mesh::addCube(Mesh &mesh, float centerX, float centerY, float centerZ, float width,
                   float height, float depth, bool projectedUVs)
{
    const float extentX = width * 0.5f;
    const float extentY = height * 0.5f;
    const float extentZ = depth * 0.5f;

    struct Face
    {
        fastgltf::math::fvec3 n;
        fastgltf::math::fvec3 v0, v1, v2, v3;
    };

    std::array<Face, 6> faces = {{// -X
                                  {.n = {-1, 0, 0},
                                   .v0 = {-extentX, -extentY, -extentZ},
                                   .v1 = {-extentX, -extentY, extentZ},
                                   .v2 = {-extentX, extentY, extentZ},
                                   .v3 = {-extentX, extentY, -extentZ}},
                                  // +X
                                  {.n = {1, 0, 0},
                                   .v0 = {extentX, -extentY, extentZ},
                                   .v1 = {extentX, -extentY, -extentZ},
                                   .v2 = {extentX, extentY, -extentZ},
                                   .v3 = {extentX, extentY, extentZ}},
                                  // -Y
                                  {.n = {0, -1, 0},
                                   .v0 = {-extentX, -extentY, -extentZ},
                                   .v1 = {extentX, -extentY, -extentZ},
                                   .v2 = {extentX, -extentY, extentZ},
                                   .v3 = {-extentX, -extentY, extentZ}},
                                  // +Y
                                  {.n = {0, 1, 0},
                                   .v0 = {-extentX, extentY, extentZ},
                                   .v1 = {extentX, extentY, extentZ},
                                   .v2 = {extentX, extentY, -extentZ},
                                   .v3 = {-extentX, extentY, -extentZ}},
                                  // -Z
                                  {.n = {0, 0, -1},
                                   .v0 = {extentX, -extentY, -extentZ},
                                   .v1 = {-extentX, -extentY, -extentZ},
                                   .v2 = {-extentX, extentY, -extentZ},
                                   .v3 = {extentX, extentY, -extentZ}},
                                  // +Z
                                  {.n = {0, 0, 1},
                                   .v0 = {-extentX, -extentY, extentZ},
                                   .v1 = {extentX, -extentY, extentZ},
                                   .v2 = {extentX, extentY, extentZ},
                                   .v3 = {-extentX, extentY, extentZ}}}};

    auto startIndex = static_cast<uint32_t>(mesh.vertices.size());

    mesh.vertices.reserve(mesh.vertices.size() + 24);
    mesh.indices.reserve(mesh.indices.size() + 36);

    mesh.lod = MeshLOD::LOD_ALL;
    IslandMap::Island &island = mesh.islands.getOrCreateIsland(bx::Vec3(centerX, centerY, centerZ));
    island.boundsMin = bx::Vec3(centerX - extentX, centerY - extentY, centerZ - extentZ);
    island.boundsMax = bx::Vec3(centerX + extentX, centerY + extentY, centerZ + extentZ);

    for (int faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        auto &face = faces[faceIndex];

        const fastgltf::math::fvec3 corners[4] = {face.v0, face.v1, face.v2, face.v3};

        MeshVertex vertices[4];

        for (int i = 0; i < 4; ++i)
        {
            vertices[i].x = centerX + corners[i].x();
            vertices[i].y = centerY + corners[i].y();
            vertices[i].z = centerZ + corners[i].z();

            vertices[i].normal = encodeNormalRgba8(face.n, 0.0f);

            if (projectedUVs)
            {
                // Projected UVs from +X face
                vertices[i].u = corners[i].y() / height + 0.5f;
                vertices[i].v = -corners[i].z() / depth + 0.5f;
            }
            else
            {
                vertices[i].u = (i == 1 || i == 2) ? 1.0f : 0.0f;
                vertices[i].v = (i >= 2) ? 1.0f : 0.0f;
            }

            mesh.vertices.push_back(vertices[i]);
        }

        uint32_t base = startIndex + faceIndex * 4;

        // two triangles per face
        mesh.indices.insert(mesh.indices.end(),
                            {base + 0, base + 1, base + 2, base + 0, base + 2, base + 3});
    }
}