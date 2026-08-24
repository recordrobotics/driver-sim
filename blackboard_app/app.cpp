#include "app.h"

#include "gui.h"
#include "logger.h"
#include "platform/imgui_impl_sdl_bgfx.h"
#include "renderer.h"
#include "resources.h"
#include "window.h"

#include <SDL3/SDL.h>
#include <bgfx/bgfx.h>
#include <imgui/backends/imgui_impl_sdl3.h>
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <iostream>
#include <limits>

namespace blackboard::app
{
    namespace
    {
        constexpr float WINDOW_WORK_AREA_RATIO = 0.7f;
    }

    App::App(const char *app_name, const renderer::Api renderer_api,
             const std::function<void()> &on_init, const std::function<void()> &on_update,
             const std::function<void()> &after_events, const uint16_t width, const uint16_t height,
             const bool fullscreen, const bool vsync)
        : main_window{std::make_unique<Window>()}, m_renderer_api{renderer_api}, on_init{on_init},
          on_update{on_update}, after_events{after_events}
    {
        if (!SDL_Init(SDL_INIT_VIDEO))
        {
            logger::logger->error("Error initializing SDL: %s", SDL_GetError());
            return;
        }

        main_window->title = app_name;

        SDL_Rect usable_bounds{};
        const SDL_DisplayID primary_display = SDL_GetPrimaryDisplay();
        if (primary_display != 0 && SDL_GetDisplayUsableBounds(primary_display, &usable_bounds))
        {
            const int target_width =
                static_cast<int>(static_cast<float>(usable_bounds.w) * WINDOW_WORK_AREA_RATIO);
            const int target_height =
                static_cast<int>(static_cast<float>(usable_bounds.h) * WINDOW_WORK_AREA_RATIO);

            const int clamped_width =
                std::clamp(target_width, 1, static_cast<int>(std::numeric_limits<uint16_t>::max()));
            const int clamped_height = std::clamp(
                target_height, 1, static_cast<int>(std::numeric_limits<uint16_t>::max()));

            main_window->width = static_cast<uint16_t>(clamped_width);
            main_window->height = static_cast<uint16_t>(clamped_height);
        }
        else
        {
            main_window->width = width;
            main_window->height = height;
        }

        main_window->fullscreen = fullscreen;
        main_window->vsync = vsync;
        main_window->init_platform_window();

        gui::init();
        if (!renderer::init(*main_window, m_renderer_api, main_window->width, main_window->height,
                            main_window->vsync))
        {
            logger::logger->error("Renderer not initialized");
            main_window.reset();
            return;
        }

        switch (m_renderer_api)
        {
        case renderer::Api::METAL:
        {
            ImGui_ImplSDL3_InitForMetal(main_window->window);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "metal");
        }
        break;
        case renderer::Api::D3D11:
        {
            ImGui_ImplSDL3_InitForD3D(main_window->window);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");
        }
        break;
        case renderer::Api::D3D12:
        {
            ImGui_ImplSDL3_InitForD3D(main_window->window);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d12");
        }
        break;
        case renderer::Api::VULKAN:
        {
            ImGui_ImplSDL3_InitForVulkan(main_window->window);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
        }
        break;
        case renderer::Api::OPENGL:
        {
            // no context so vulkan
            ImGui_ImplSDL3_InitForVulkan(main_window->window);
            SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
        }
        break;
        case renderer::Api::NONE:
            logger::logger->info("Render context not initialized");
            break;
        case renderer::Api::WEBGL:
            logger::logger->info("WebGL not supported");
            break;
        case renderer::Api::AUTO:
            // no hints for auto
            break;
        }

        logger::logger->info("Ending App constructor");
    }

    void App::run()
    {
        on_init();

        if (ImGui::GetCurrentContext() != nullptr && main_window)
        {
            ImGui::LoadIniSettingsFromDisk(gui::get_ini_path().c_str());

            SDL_Event event;
            while (running)
            {
                while (main_window->window != nullptr && SDL_PollEvent(&event))
                {
                    ImGui_ImplSDL3_ProcessEvent(&event);
                    const bool is_main_window =
                        event.window.windowID == SDL_GetWindowID(main_window->window);

                    if (event.type == SDL_EVENT_KEY_DOWN && is_main_window)
                    {
#ifdef _DEBUG
                        if (event.key.key == SDLK_F10)
                        {
                            renderer::set_bgfx_debug_flags(renderer::get_bgfx_debug_flags() ^
                                                           (BGFX_DEBUG_TEXT | BGFX_DEBUG_STATS));
                        }
#endif
                    }

                    if (event.type == SDL_EVENT_QUIT ||
                        (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && is_main_window))
                    {
                        running = false;
                    }
                    else if (event.type == SDL_EVENT_WINDOW_RESIZED && is_main_window)
                    {
                        const auto width = event.window.data1;
                        const auto height = event.window.data2;
                        main_window->width = width;
                        main_window->height = height;
                        renderer::ImGui_Impl_sdl_bgfx_Resize(main_window->window);
                    }
                }

                after_events();

                renderer::ImGui_Impl_sdl_bgfx_NewFrame();
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();

                on_update();

                ImGui::Render();
                renderer::ImGui_Impl_sdl_bgfx_Render(main_window->imgui_view_id,
                                                     ImGui::GetDrawData(), 0);

                if (const auto io = ImGui::GetIO();
                    io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                {
                    ImGui::UpdatePlatformWindows();
                    ImGui::RenderPlatformWindowsDefault();
                }

                bgfx::frame();
            }
        }
        else
        {
            while (running)
            {
                on_update();
            }
        }
    }

    App::~App()
    {
        if (blackboard::gui::isInit())
        {
            ImGui::SaveIniSettingsToDisk(gui::get_ini_path().c_str());

            ImGui_ImplSDL3_Shutdown();
            renderer::ImGui_Impl_sdl_bgfx_Shutdown();

            ImGui::DestroyContext();
            bgfx::shutdown();

            SDL_DestroyWindow(main_window->window);
            SDL_Quit();
        }
        main_window.reset();
        logger::shutdown();
    }

} // namespace blackboard::app
