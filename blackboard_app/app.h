#pragma once
#include "renderer.h"
#include "window.h"

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>

namespace blackboard::app
{
    class App
    {
      public:
        static constexpr uint16_t DEFAULT_WIDTH = 1280u;
        static constexpr uint16_t DEFAULT_HEIGHT = 720u;

        App(const char *app_name, renderer::Api renderer_api, const std::function<void()> &on_init,
            const std::function<void()> &on_update, uint16_t width = DEFAULT_WIDTH,
            uint16_t height = DEFAULT_HEIGHT, bool fullscreen = false);
        ~App();

        App(const App &) = delete;
        App &operator=(const App &) = delete;
        App(App &&) = delete;
        App &operator=(App &&) = delete;

        void run();

        [[nodiscard]] renderer::Api get_renderer_api() const { return m_renderer_api; }

        Window *get_main_window() { return main_window.get(); }

      private:
        std::function<void()> on_init;
        std::function<void()> on_update;

        bool running{true};
        std::unique_ptr<Window> main_window;

        renderer::Api m_renderer_api{renderer::Api::NONE};
    };

} // namespace blackboard::app
