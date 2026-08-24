#pragma once
#include <cstdint>
#include <stdint.h>

struct SDL_Window;

namespace blackboard::app
{
    struct Window;
}

namespace blackboard::renderer
{

    enum class Api : uint8_t
    {
        NONE = 0,
        METAL,
        D3D11,
        D3D12,
        VULKAN,
        OPENGL,
        WEBGL,
        AUTO
    };

    bool init(app::Window &window, Api &renderer_api, uint16_t width, uint16_t height, bool vsync);

    uint32_t get_bgfx_debug_flags();
    void set_bgfx_debug_flags(uint32_t flags);

} // namespace blackboard::renderer
