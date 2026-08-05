#pragma once
#include <bgfx/bgfx.h>
#include <bx/bx.h>
#include <imgui/imgui.h>
#include <cstdint>

struct SDL_Window;
typedef uint32_t SDL_WindowID;

namespace blackboard::renderer
{
#define IMGUI_FLAGS_NONE UINT8_C(0x00)
#define IMGUI_FLAGS_ALPHA_BLEND UINT8_C(0x01)

    struct TextureBgfx
    {
        bgfx::TextureHandle handle;
        uint8_t flags;
        uint8_t mip;
        uint32_t unused;
    };

    ///
    inline ImTextureID toId(bgfx::TextureHandle _handle, uint8_t _flags, uint8_t _mip)
    {
        TextureBgfx tex{
            .handle = _handle,
            .flags = _flags,
            .mip = _mip,
            .unused = 0,
        };

        return bx::bitCast<ImTextureID>(tex);
    }

    // Helper function for passing bgfx::TextureHandle to ImGui::Image.
    inline void Image(bgfx::TextureHandle _handle, uint8_t _flags, uint8_t _mip, const ImVec2 &_size, const ImVec2 &_uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &_uv1 = ImVec2(1.0f, 1.0f), const ImVec4 &_tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), const ImVec4 &_borderCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
    {
        ImGui::ImageWithBg(toId(_handle, _flags, _mip), _size, _uv0, _uv1, _borderCol, _tintCol);
    }

    // Helper function for passing bgfx::TextureHandle to ImGui::Image.
    inline void Image(bgfx::TextureHandle _handle, const ImVec2 &_size, const ImVec2 &_uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &_uv1 = ImVec2(1.0f, 1.0f), const ImVec4 &_tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f), const ImVec4 &_borderCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f))
    {
        Image(_handle, IMGUI_FLAGS_ALPHA_BLEND, 0, _size, _uv0, _uv1, _tintCol, _borderCol);
    }

    // Helper function for passing bgfx::TextureHandle to ImGui::ImageButton.
    inline bool ImageButton(bgfx::TextureHandle _handle, uint8_t _flags, uint8_t _mip, const ImVec2 &_size, const ImVec2 &_uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &_uv1 = ImVec2(1.0f, 1.0f), const ImVec4 &_bgCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f), const ImVec4 &_tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
    {
        return ImGui::ImageButton("image", toId(_handle, _flags, _mip), _size, _uv0, _uv1, _bgCol, _tintCol);
    }

    // Helper function for passing bgfx::TextureHandle to ImGui::ImageButton.
    inline bool ImageButton(bgfx::TextureHandle _handle, const ImVec2 &_size, const ImVec2 &_uv0 = ImVec2(0.0f, 0.0f), const ImVec2 &_uv1 = ImVec2(1.0f, 1.0f), const ImVec4 &_bgCol = ImVec4(0.0f, 0.0f, 0.0f, 0.0f), const ImVec4 &_tintCol = ImVec4(1.0f, 1.0f, 1.0f, 1.0f))
    {
        return ImageButton(_handle, IMGUI_FLAGS_ALPHA_BLEND, 0, _size, _uv0, _uv1, _bgCol, _tintCol);
    }

    void ImGui_Impl_sdl_bgfx_Init(int view);
    void ImGui_Impl_sdl_bgfx_Shutdown();
    void ImGui_Impl_sdl_bgfx_NewFrame();
    void ImGui_Impl_sdl_bgfx_Resize(SDL_Window *);
    void ImGui_Impl_sdl_bgfx_UpdateTextures(ImVector<ImTextureData *> *Textures);
    void ImGui_Impl_sdl_bgfx_Render(const bgfx::ViewId viewId, ImDrawData *draw_data, uint32_t clearColor);

    // Use if you want to reset your rendering device without losing ImGui state.
    void ImGui_Impl_sdl_bgfx_InvalidateDeviceObjects();
    bool ImGui_Impl_sdl_bgfx_CreateDeviceObjects();

    void *native_window_handle(ImGuiViewport *viewport, SDL_WindowID window_id);
} // namespace blackboard::renderer
