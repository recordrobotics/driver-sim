#pragma once
#include <cstdint>
#include <string>
#include <utility>

struct SDL_Window;

namespace blackboard::app
{

    struct Window
    {
        static constexpr uint16_t DEFAULT_WIDTH = 1280u;
        static constexpr uint16_t DEFAULT_HEIGHT = 720u;
        static constexpr uint16_t IMGUI_VIEW_ID = 255;

        ~Window();
        Window() = default;
        Window(const Window &) = delete;
        Window &operator=(const Window &) = delete;
        Window(Window &&) = delete;
        Window &operator=(Window &&) = delete;

        void init_platform_window();

        [[nodiscard]] std::pair<uint16_t, uint16_t> get_size_in_pixels() const;
        [[nodiscard]] float effective_display_resolution() const;

        // get position
        [[nodiscard]] std::pair<uint16_t, uint16_t> get_position() const;

        std::string title{"title"};
        uint16_t width{DEFAULT_WIDTH};
        uint16_t height{DEFAULT_HEIGHT};
        uint16_t imgui_view_id{IMGUI_VIEW_ID}; // might be possible to remove this id
        bool fullscreen{false};
        SDL_Window *window{nullptr};
        bool is_dragging{false};
    };

} // namespace blackboard::app
