#include "window.h"

#include "gui.h"
#include "logger.h"

#include <SDL3/SDL.h>

#include <bimg/decode.h>
#include <bx/allocator.h>

#include <iostream>
#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif // _WIN32

#include <app.png.h>

namespace blackboard::app
{
    Window::~Window() { logger::logger->info("Window {} destroyed", title); }

    static bx::DefaultAllocator s_allocator;

    void Window::init_platform_window()
    {
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, title.c_str());
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, SDL_WINDOWPOS_CENTERED);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, SDL_WINDOWPOS_CENTERED);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
        SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
        window = SDL_CreateWindowWithProperties(props);
        SDL_DestroyProperties(props);

        bimg::ImageContainer *image = bimg::imageParse(&s_allocator, (void *)app_png_bytes,
                                                       static_cast<uint32_t>(sizeof(app_png_bytes)),
                                                       bimg::TextureFormat::BGRA8);

        if (image == nullptr)
        {
            logger::logger->error("Could not load icon");
        }
        else
        {
            if (image->m_width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
                image->m_height > static_cast<uint32_t>(std::numeric_limits<int>::max()))
            {
                logger::logger->error("Icon dimensions are too large");
                bimg::imageFree(image);
                return;
            }

            const int icon_width = static_cast<int>(image->m_width);
            const int icon_height = static_cast<int>(image->m_height);
            const int icon_pitch = icon_width * 4;

            SDL_Surface *surface = SDL_CreateSurfaceFrom(
                icon_width, icon_height, SDL_PIXELFORMAT_ARGB8888, image->m_data, icon_pitch);

            if (surface != nullptr)
            {
                SDL_SetWindowIcon(window, surface);
                SDL_DestroySurface(surface);
            }
            else
            {
                logger::logger->error("Could not create surface for icon");
            }

            bimg::imageFree(image);
        }
    }

    std::pair<uint16_t, uint16_t> Window::get_size_in_pixels() const
    {
        int width{0u};
        int height{0u};
        SDL_GetWindowSizeInPixels(window, &width, &height);
        return {width, height};
    }

    float Window::effective_display_resolution() const
    {
        const auto pixel_density = SDL_GetWindowPixelDensity(window);
        const auto display_scale = SDL_GetWindowDisplayScale(window);
        return pixel_density * display_scale * gui::STANDARD_DPI;
    }

    std::pair<uint16_t, uint16_t> Window::get_position() const
    {
        int xPos{0u};
        int yPos{0u};
        SDL_GetWindowPosition(window, &xPos, &yPos);
        return {xPos, yPos};
    }

} // namespace blackboard::app
