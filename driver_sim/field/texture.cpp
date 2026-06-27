#include <cmath>
#include <stdexcept>
#include <blackboard_app/logger.h>
#include "texture.h"
#include <format>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/math.h>

using namespace blackboard::logger;

static bx::DefaultAllocator s_allocator;

inline constexpr uint8_t calcNumMips(bool _hasMips, uint16_t _width, uint16_t _height, uint16_t _depth = 1)
{
    if (_hasMips)
    {
        const uint32_t max = bx::max(_width, _height, _depth);
        const uint32_t num = 1 + bx::floorLog2(max);

        return uint8_t(num);
    }

    return 1;
}

Texture::Texture(std::string name, uint16_t viewWidth, uint16_t viewHeight, float widthFraction, float heightFraction, bool hasMips, uint16_t numLayers, bgfx::TextureFormat::Enum format, uint64_t flags)
    : name(name), width(static_cast<uint16_t>(floorf(viewWidth * widthFraction))), height(static_cast<uint16_t>(floorf(viewHeight * heightFraction))), widthFraction(widthFraction), heightFraction(heightFraction), hasMips(hasMips), numLayers(numLayers), format(format), flags(flags), hasResized(false)
{
    handle = bgfx::createTexture2D(
        this->width, this->height,
        hasMips,
        numLayers,
        format,
        flags);

    if (!bgfx::isValid(handle))
    {
        logger->error("Failed to create texture '{0}'.", name);
        throw std::runtime_error(std::format("Failed to create texture '{0}'.", name));
    }

    mipCount = calcNumMips(this->hasMips, this->width, this->height, this->numLayers);
}

Texture::Texture(std::string name, void *image_data, int image_data_size, uint64_t _flags)
    : name(name), widthFraction(1.0f), heightFraction(1.0f), hasMips(false), numLayers(1), format(bgfx::TextureFormat::BGRA8), flags(_flags), hasResized(false)
{
    bimg::ImageContainer *image = bimg::imageParse(
        &s_allocator,
        image_data,
        static_cast<uint32_t>(image_data_size),
        bimg::TextureFormat::BGRA8);

    if (image == nullptr)
    {
        logger->error("Failed to load image '{0}'", name);
        throw std::runtime_error(std::format("Failed to load image '{0}'", name));
    }

    this->width = static_cast<uint16_t>(image->m_width);
    this->height = static_cast<uint16_t>(image->m_height);

    handle = bgfx::createTexture2D(
        static_cast<uint16_t>(image->m_width),
        static_cast<uint16_t>(image->m_height),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        _flags,
        bgfx::copy(image->m_data, image->m_size));

    bimg::imageFree(image);

    if (!bgfx::isValid(handle))
    {
        logger->error("Failed to create texture '{0}'", name);
        throw std::runtime_error(std::format("Failed to create texture '{0}'", name));
    }

    mipCount = calcNumMips(this->hasMips, this->width, this->height, this->numLayers);
}

void Texture::destroy()
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
}

void Texture::ensure(uint16_t viewWidth, uint16_t viewHeight)
{
    uint16_t newWidth = static_cast<uint16_t>(floorf(viewWidth * widthFraction));
    uint16_t newHeight = static_cast<uint16_t>(floorf(viewHeight * heightFraction));

    if (newWidth != width || newHeight != height)
    {
        width = newWidth;
        height = newHeight;

        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
        }

        handle = bgfx::createTexture2D(
            width, height,
            hasMips,
            numLayers,
            format,
            flags);

        logger->info("Resized texture '{0}' to {1}x{2}", name, width, height);

        if (!bgfx::isValid(handle))
        {
            logger->error("Failed to create texture '{0}'.", name);
            throw std::runtime_error(std::format("Failed to create texture '{0}'.", name));
        }

        mipCount = calcNumMips(this->hasMips, this->width, this->height, this->numLayers);

        hasResized = true;
    }
}

void Texture::beginFrame()
{
    hasResized = false;
}

FrameBuffer::FrameBuffer(std::string name, std::vector<Texture *> attachments)
    : name(name), attachments(attachments)
{
    attachmentHandles = std::vector<bgfx::TextureHandle>(attachments.size());
    for (size_t i = 0; i < attachments.size(); i++)
    {
        attachmentHandles[i] = attachments[i]->handle;
    }

    handle = bgfx::createFrameBuffer(attachmentHandles.size(), attachmentHandles.data(), false);
    if (!bgfx::isValid(handle))
    {
        logger->error("Failed to create framebuffer '{0}'.", name);
        throw std::runtime_error(std::format("Failed to create framebuffer '{0}'.", name));
    }

    logger->info("Created framebuffer '{0}' with {1} attachments.", name, attachments.size());
}

bool FrameBuffer::ensure(uint16_t viewWidth, uint16_t viewHeight)
{
    bool resized = false;
    for (size_t i = 0; i < attachments.size(); i++)
    {
        attachments[i]->ensure(viewWidth, viewHeight);
        if (attachments[i]->hasResized)
        {
            resized = true;
        }
    }

    if (resized)
    {
        if (bgfx::isValid(handle))
        {
            bgfx::destroy(handle);
        }

        for (size_t i = 0; i < attachments.size(); i++)
        {
            attachmentHandles[i] = attachments[i]->handle;
        }

        handle = bgfx::createFrameBuffer(attachmentHandles.size(), attachmentHandles.data(), false);

        if (!bgfx::isValid(handle))
        {
            logger->error("Failed to create framebuffer '{0}'.", name);
            throw std::runtime_error(std::format("Failed to create framebuffer '{0}'.", name));
        }

        logger->info("Recreated framebuffer '{0}' with {1} attachments.", name, attachments.size());
    }

    return resized;
}

void FrameBuffer::destroy()
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }

    attachmentHandles.clear();
}