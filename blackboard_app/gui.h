#pragma once

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <string>
#include <cstdint>
#include <array>

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

    bool load_image(void *image_data, int image_data_size, ImTexture &out_texture, uint64_t _flags = 0);

    ImFont *get_font(const std::string &font_name);

    constexpr unsigned hex_digit(char c)
    {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        if (c >= 'A' && c <= 'F')
            return c - 'A' + 10;
        throw "invalid hex digit";
    }

    constexpr unsigned hex_byte(char hi, char lo)
    {
        return (hex_digit(hi) << 4) | hex_digit(lo);
    }

    // Input format: #RRGGBBAA
    constexpr ImVec4 string_hex_to_rgba_float(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
            throw "expected #RRGGBBAA";

        return {
            hex_byte(color[1], color[2]) / 255.0f,
            hex_byte(color[3], color[4]) / 255.0f,
            hex_byte(color[5], color[6]) / 255.0f,
            hex_byte(color[7], color[8]) / 255.0f};
    }
    
    // Input format: #RRGGBBAA
    constexpr std::array<float, 4> string_hex_to_rgba_float_array(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
            throw "expected #RRGGBBAA";

        return {
            hex_byte(color[1], color[2]) / 255.0f,
            hex_byte(color[3], color[4]) / 255.0f,
            hex_byte(color[5], color[6]) / 255.0f,
            hex_byte(color[7], color[8]) / 255.0f};
    }

    // Input format: #RRGGBBAA
    constexpr ImU32 string_hex_to_rgba_u32(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
            throw "expected #RRGGBBAA";
        return IM_COL32(
            hex_byte(color[1], color[2]),
            hex_byte(color[3], color[4]),
            hex_byte(color[5], color[6]),
            hex_byte(color[7], color[8]));
    }

    extern std::string imgui_ini_path;
} // namespace blackboard::gui
