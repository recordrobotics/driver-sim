#pragma once

#include <bgfx/bgfx.h>
#include <cstddef>
#include "../utils.h"
#include <vector>

#include <spdlog/fmt/fmt.h>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

enum class MaterialType
{
    Opaque,
    Transparent
};

namespace fmt
{
    template <>
    struct formatter<MaterialType> : formatter<string_view>
    {
        template <typename FormatContext>
        auto format(MaterialType type, FormatContext &ctx) const
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

typedef struct Material
{
    MaterialType type;
    std::array<float, 4> baseColor;
    bool writesObjectMotionVectors;
    float metallic;
    float roughness;

    Material() : type(MaterialType::Opaque), baseColor{1.0f, 0.0f, 1.0f, 1.0f}, writesObjectMotionVectors(false), metallic(0.0f), roughness(0.5f)
    {
    }

    Material(const fastgltf::Material &m)
    {
        auto &base = m.pbrData.baseColorFactor;

        baseColor[0] = base[0];
        baseColor[1] = base[1];
        baseColor[2] = base[2];
        baseColor[3] = base[3];

        type = m.alphaMode == fastgltf::AlphaMode::Blend ? MaterialType::Transparent : MaterialType::Opaque;
        writesObjectMotionVectors = false;
        metallic = m.pbrData.metallicFactor;
        roughness = m.pbrData.roughnessFactor;
    }

    bool operator==(const Material &other) const
    {
        return bit_equal(baseColor[0], other.baseColor[0]) &&
               bit_equal(baseColor[1], other.baseColor[1]) &&
               bit_equal(baseColor[2], other.baseColor[2]) &&
               bit_equal(baseColor[3], other.baseColor[3]) &&
               type == other.type;
    }
} Material;

typedef struct MaterialHash
{
    std::size_t operator()(const Material &mat) const
    {
        std::size_t h = 2166136261u;
        for (std::size_t i = 0; i < mat.baseColor.size(); ++i)
        {
            hash_combine(h, mat.baseColor[i]);
        }

        hash_combine(h, mat.type);

        return h;
    }
} MaterialHash;

typedef struct MeshVertex
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
} MeshVertex;

typedef struct Mesh
{
    Material material;

    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    bgfx::VertexBufferHandle vertexBuffer = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle indexBuffer = BGFX_INVALID_HANDLE;

    Mesh() = default;
    Mesh(const Material &mat) : material(mat) {}

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
                bgfx::makeRef(indices.data(), indices.size() * sizeof(uint32_t)), BGFX_BUFFER_INDEX32);
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

    static void fromGltfModel(std::vector<Mesh> &meshesOut, const fastgltf::Asset &asset);
} Mesh;