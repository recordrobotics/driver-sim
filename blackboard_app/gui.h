#pragma once

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <array>
#include <cstdint>
#include <string>

namespace blackboard::gui
{
    struct ImTexture
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
    };

    constexpr float STANDARD_DPI = 96.0f;
    constexpr int OVERSAMPLE_DEFAULT = 4;
    constexpr float RASTERIZER_MULTIPLY_DEFAULT = 1.25f;

    void init();

    bool isInit();

    void set_blender_theme();
    void set_blackboard_theme();

    void dockspace();

    /// @brief Load a font inside the collection of gui fonts
    bool load_font(const std::string &font_name, const void *font_data, int font_data_size,
                   float size, float ddpi, bool set_as_default = false,
                   int oversample_h = OVERSAMPLE_DEFAULT, int oversample_v = OVERSAMPLE_DEFAULT,
                   float rasterizer_multiply = RASTERIZER_MULTIPLY_DEFAULT);

    bool load_image(const void *image_data, int image_data_size, ImTexture &out_texture,
                    uint64_t _flags = 0);

    ImFont *get_font(const std::string &font_name);

    std::string &get_ini_path();

    // NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

    constexpr uint8_t hex_digit(char chr)
    {
        if (chr >= '0' && chr <= '9')
        {
            return chr - '0';
        }
        if (chr >= 'a' && chr <= 'f')
        {
            return chr - 'a' + 10;
        }
        if (chr >= 'A' && chr <= 'F')
        {
            return chr - 'A' + 10;
        }
        throw "invalid hex digit";
    }

    constexpr uint8_t hex_byte(char high, char low)
    {
        return (hex_digit(high) << 4) | hex_digit(low);
    }

    // Input format: #RRGGBBAA
    constexpr ImVec4 string_hex_to_rgba_float(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
        {
            throw "expected #RRGGBBAA";
        }

        return {static_cast<float>(hex_byte(color[1], color[2])) / 255.0f,
                static_cast<float>(hex_byte(color[3], color[4])) / 255.0f,
                static_cast<float>(hex_byte(color[5], color[6])) / 255.0f,
                static_cast<float>(hex_byte(color[7], color[8])) / 255.0f};
    }

    // Input format: #RRGGBBAA
    constexpr std::array<float, 4> string_hex_to_rgba_float_array(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
        {
            throw "expected #RRGGBBAA";
        }

        return {static_cast<float>(hex_byte(color[1], color[2])) / 255.0f,
                static_cast<float>(hex_byte(color[3], color[4])) / 255.0f,
                static_cast<float>(hex_byte(color[5], color[6])) / 255.0f,
                static_cast<float>(hex_byte(color[7], color[8])) / 255.0f};
    }

    // Input format: #RRGGBBAA
    constexpr ImU32 string_hex_to_rgba_u32(std::string_view color)
    {
        if (color.size() != 9 || color[0] != '#')
        {
            throw "expected #RRGGBBAA";
        }

        return IM_COL32(hex_byte(color[1], color[2]), hex_byte(color[3], color[4]),
                        hex_byte(color[5], color[6]), hex_byte(color[7], color[8]));
    }

    constexpr ImU32 u32_multiply_alpha(ImU32 color, float alpha)
    {
        float colorAlpha = static_cast<float>((color >> 24) & 0xFF) / 255.0f;
        float newAlpha = colorAlpha * alpha;
        return (color & 0x00FFFFFF) | (static_cast<ImU32>(newAlpha * 255.0f) << 24);
    }

    // NOLINTEND(readability-magic-numbers,cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
} // namespace blackboard::gui
