#pragma once

#include <bgfx/bgfx.h>
#include <vector>
#include <string>

typedef struct Texture
{
    std::string name;
    bgfx::TextureHandle handle;
    uint16_t width;
    uint16_t height;
    float widthFraction;
    float heightFraction;
    bool hasMips;
    uint16_t numLayers;
    bgfx::TextureFormat::Enum format;
    uint64_t flags;
    bool hasResized;

    Texture()
        : handle(BGFX_INVALID_HANDLE), width(0), height(0), widthFraction(1.0f), heightFraction(1.0f), hasMips(false), numLayers(1), format(bgfx::TextureFormat::Unknown), flags(0), hasResized(false)
    {
    }

    Texture(const Texture &other)
        : name(other.name), handle(other.handle), width(other.width), height(other.height), widthFraction(other.widthFraction), heightFraction(other.heightFraction), hasMips(other.hasMips), numLayers(other.numLayers), format(other.format), flags(other.flags), hasResized(other.hasResized)
    {
    }

    Texture(std::string name, uint16_t viewWidth, uint16_t viewHeight, float widthFraction, float heightFraction, bool hasMips, uint16_t numLayers, bgfx::TextureFormat::Enum format, uint64_t flags);

    /**
     * Ensures that the texture is the correct size for the given view dimensions.
     * @param viewWidth The width of the view to ensure the texture is sized for.
     * @param viewHeight The height of the view to ensure the texture is sized for.
     */
    void ensure(uint16_t viewWidth, uint16_t viewHeight);
    void destroy();
    void beginFrame();

} Texture;

typedef struct FrameBuffer
{
    std::string name;
    bgfx::FrameBufferHandle handle;
    std::vector<Texture *> attachments;
    std::vector<bgfx::TextureHandle> attachmentHandles;

    FrameBuffer()
        : handle(BGFX_INVALID_HANDLE)
    {
    }
    FrameBuffer(const FrameBuffer &other)
        : name(other.name), handle(other.handle), attachments(other.attachments), attachmentHandles(other.attachmentHandles)
    {
    }
    FrameBuffer(std::string name, std::vector<Texture *> attachments);

    bool ensure(uint16_t viewWidth, uint16_t viewHeight);
    void destroy();
} FrameBuffer;

#define TEXTURE(out_var, width, height, widthFraction, heightFraction, hasMips, numLayers, format, flags) \
    out_var = Texture(#out_var, width, height, widthFraction, heightFraction, hasMips, numLayers, format, flags);
#define FRAMEBUFFER(out_var, ...) \
    out_var = FrameBuffer(#out_var, {__VA_ARGS__});