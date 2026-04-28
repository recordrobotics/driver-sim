#include <cmath>
#include <stdexcept>
#include <blackboard_app/logger.h>
#include "texture.h"

using namespace blackboard::logger;

Texture::Texture(uint16_t viewWidth, uint16_t viewHeight, float widthFraction, float heightFraction, bool hasMips, uint16_t numLayers, bgfx::TextureFormat::Enum format, uint64_t flags)
    : width(static_cast<uint16_t>(floorf(viewWidth * widthFraction))), height(static_cast<uint16_t>(floorf(viewHeight * heightFraction))), widthFraction(widthFraction), heightFraction(heightFraction), hasMips(hasMips), numLayers(numLayers), format(format), flags(flags), hasResized(false)
{
    handle = bgfx::createTexture2D(
        this->width, this->height,
        hasMips,
        numLayers,
        format,
        flags);

    if (!bgfx::isValid(handle))
    {
        logger->error("Failed to create texture.");
        throw std::runtime_error("Failed to create texture.");
    }
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

        if (!bgfx::isValid(handle))
        {
            logger->error("Failed to create texture.");
            throw std::runtime_error("Failed to create texture.");
        }

        hasResized = true;
    }
}

void Texture::beginFrame()
{
    hasResized = false;
}

FrameBuffer::FrameBuffer(std::vector<Texture> attachments)
    : attachments(attachments)
{
    attachmentHandles = std::vector<bgfx::TextureHandle>(attachments.size());
    for (size_t i = 0; i < attachments.size(); i++)
    {
        attachmentHandles[i] = attachments[i].handle;
    }

    handle = bgfx::createFrameBuffer(attachmentHandles.size(), attachmentHandles.data(), false);
    if (!bgfx::isValid(handle))
    {
        logger->error("Failed to create framebuffer.");
        throw std::runtime_error("Failed to create framebuffer.");
    }
}

bool FrameBuffer::ensure(uint16_t viewWidth, uint16_t viewHeight)
{
    bool resized = false;
    for (size_t i = 0; i < attachments.size(); i++)
    {
        attachments[i].ensure(viewWidth, viewHeight);
        if (attachments[i].hasResized)
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
            attachmentHandles[i] = attachments[i].handle;
        }

        handle = bgfx::createFrameBuffer(attachmentHandles.size(), attachmentHandles.data(), false);

        if (!bgfx::isValid(handle))
        {
            logger->error("Failed to create framebuffer.");
            throw std::runtime_error("Failed to create framebuffer.");
        }
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