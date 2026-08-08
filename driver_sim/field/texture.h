#pragma once

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <string>
#include <vector>

struct Texture
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
    uint8_t mipCount;

    Texture()
        : handle(BGFX_INVALID_HANDLE), width(0), height(0), widthFraction(1.0f),
          heightFraction(1.0f), hasMips(false), numLayers(1), format(bgfx::TextureFormat::Unknown),
          flags(0), hasResized(false), mipCount(0)
    {
    }

    ~Texture() = default;

    Texture(const Texture &other) = default;
    Texture &operator=(const Texture &) = default;
    Texture(Texture &&other) = default;
    Texture &operator=(Texture &&) noexcept = default;

    Texture(std::string name, uint16_t viewWidth, uint16_t viewHeight, float widthFraction,
            float heightFraction, bool hasMips, uint16_t numLayers,
            bgfx::TextureFormat::Enum format, uint64_t flags, const bgfx::Memory *_mem = nullptr);

    Texture(std::string name, void *image_data, int image_data_size,
            bimg::TextureFormat::Enum imageFormat, bgfx::TextureFormat::Enum format,
            uint64_t _flags);

    /**
     * Ensures that the texture is the correct size for the given view dimensions.
     * @param viewWidth The width of the view to ensure the texture is sized for.
     * @param viewHeight The height of the view to ensure the texture is sized for.
     */
    void ensure(uint16_t viewWidth, uint16_t viewHeight);
    void destroy();
    void beginFrame();
};

struct FrameBuffer
{
    std::string name;
    bgfx::FrameBufferHandle handle;
    std::vector<Texture *> attachments;
    std::vector<bgfx::TextureHandle> attachmentHandles;

    FrameBuffer() : handle(BGFX_INVALID_HANDLE) {}
    ~FrameBuffer() = default;
    FrameBuffer(const FrameBuffer &other) = default;
    FrameBuffer &operator=(const FrameBuffer &) = delete;
    FrameBuffer(FrameBuffer &&other) = default;
    FrameBuffer &operator=(FrameBuffer &&) noexcept = default;
    FrameBuffer(std::string name, std::vector<Texture *> attachments);

    bool ensure(uint16_t viewWidth, uint16_t viewHeight);
    void destroy();
};

#define TEXTURE(out_var, width, height, widthFraction, heightFraction, hasMips, numLayers, format, \
                flags)                                                                             \
    out_var = Texture(#out_var, width, height, widthFraction, heightFraction, hasMips, numLayers,  \
                      format, flags);
#define TEXTURE_MEMORY(out_var, width, height, widthFraction, heightFraction, hasMips, numLayers,  \
                       format, flags, mem)                                                         \
    out_var = Texture(#out_var, width, height, widthFraction, heightFraction, hasMips, numLayers,  \
                      format, flags, mem);
#define TEXTURE_EMBEDDED(out_var, image, imageFormat, format, flags)                               \
    out_var = Texture(#out_var, (void *)image##_bytes, sizeof(image##_bytes), imageFormat, format, \
                      flags);
#define FRAMEBUFFER(out_var, ...) out_var = FrameBuffer(#out_var, {__VA_ARGS__});