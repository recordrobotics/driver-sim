#include "renderer.h"

#include "platform/imgui_impl_sdl_bgfx.h"
#include "window.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <blackboard_app/logger.h>

#include <iostream>
#include <utility>

namespace blackboard::renderer
{
    static uint32_t bgfx_debug_flags;

    bool init(app::Window &window, Api &renderer_api, const uint16_t width, const uint16_t height)
    {
        ImGui_Impl_sdl_bgfx_Init(window.imgui_view_id);

        auto window_handle{
            native_window_handle(ImGui::GetMainViewport(), SDL_GetWindowID(window.window))};
        if (!window_handle)
        {
            logger::logger->error(SDL_GetError());
            return false;
        }

        bgfx::Init bgfx_init;
        bgfx::renderFrame(); // single threaded mode
        switch (renderer_api)
        {
        case Api::METAL:
            bgfx_init.type = bgfx::RendererType::Metal; // auto choose renderer
            break;
        case Api::D3D11:
            bgfx_init.type = bgfx::RendererType::Direct3D11; // auto choose renderer
            break;
        case Api::D3D12:
            bgfx_init.type = bgfx::RendererType::Direct3D12; // auto choose renderer
            break;
        case Api::VULKAN:
            bgfx_init.type = bgfx::RendererType::Vulkan; // auto choose renderer
            break;
        case Api::WEBGL:
        case Api::OPENGL:
            bgfx_init.type = bgfx::RendererType::OpenGL; // auto choose renderer
            break;
        default:
            bgfx_init.type = bgfx::RendererType::Count; // auto choose renderer
            break;
        }
        const auto [drawable_width, drawable_height] = window.get_size_in_pixels();
        bgfx_init.resolution.width = drawable_width;
        bgfx_init.resolution.height = drawable_height;
        bgfx_init.resolution.numBackBuffers = 1;
        bgfx_init.resolution.reset = BGFX_RESET_HIDPI | BGFX_RESET_VSYNC;
#ifdef SDL_VIDEO_DRIVER_X11
        bgfx_init.platformData.ndt = SDL_GetPointerProperty(
            SDL_GetWindowProperties(window.window), SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
        bgfx_init.platformData.nwh = window_handle;
#endif
        bgfx::init(bgfx_init);

#ifdef _DEBUG
        set_bgfx_debug_flags(BGFX_DEBUG_TEXT | BGFX_DEBUG_STATS);
#endif

        switch (bgfx::getRendererType())
        {
        case bgfx::RendererType::Direct3D11:
            renderer_api = Api::D3D11;
            break;
        case bgfx::RendererType::Direct3D12:
            renderer_api = Api::D3D12;
            break;
        case bgfx::RendererType::Metal:
            renderer_api = Api::METAL;
            break;
        case bgfx::RendererType::Vulkan:
            renderer_api = Api::VULKAN;
            break;
        case bgfx::RendererType::OpenGL:
            renderer_api = Api::OPENGL;
            break;
        default:
            renderer_api = Api::NONE;
            break;
        }

        return true;
    }

    uint32_t get_bgfx_debug_flags() { return bgfx_debug_flags; }

    void set_bgfx_debug_flags(uint32_t flags)
    {
        bgfx_debug_flags = flags;
        bgfx::setDebug(flags);
    }
} // namespace blackboard::renderer
