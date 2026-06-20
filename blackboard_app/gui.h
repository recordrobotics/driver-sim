#pragma once

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <string>
#include <cstdint>

namespace blackboard::gui
{
    typedef struct ImTexture
    {
        bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
        ImTextureID id = 0;

        void destroy()
        {
            if (bgfx::isValid(handle))
            {
                bgfx::destroy(handle);
                handle = BGFX_INVALID_HANDLE;
                id = 0;
            }
        }
    } ImTexture;

    constexpr float STANDARD_DPI = 96.0f;

    void init();

    bool isInit();

    void set_blender_theme();
    void set_blackboard_theme();

    void dockspace();

    /// @brief Load a font inside the collection of gui fonts
    bool load_font(const std::string &font_name, void *font_data, int font_data_size, const float size, const float ddpi, const bool set_as_default = false,
                   const int oversample_h = 4, const int oversample_v = 4, const float rasterizer_multiply = 1.25f);

    bool load_image(void *image_data, int image_data_size, ImTexture &out_texture);

    // input format #aa1199ff
    ImVec4 string_hex_to_rgba_float(const std::string &color);

    extern std::string imgui_ini_path;
} // namespace blackboard::gui
