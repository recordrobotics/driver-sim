#define IMGUI_DEFINE_MATH_OPERATORS

#define BGFX_PLATFORM_SUPPORTS_DXBC                                                                \
    (0 || BX_PLATFORM_WINDOWS || BX_PLATFORM_WINRT || BX_PLATFORM_XBOXONE)

#include "imgui_impl_sdl_bgfx.h"

#include <bgfx/bgfx.h>
#include <bgfx/embedded_shader.h>
#include <bx/math.h>
#include <bx/timer.h>
#include <imgui/imgui.h>
// #include <imgui/imgui_internal.h>
#include <SDL3/SDL.h>
#include <build_config/SDL_build_config.h>

#include <cstdint>
#include <string>
#include <vector>

#include <essl/fs_imgui_image.sc.bin.h>
#include <essl/vs_imgui_image.sc.bin.h>
#include <glsl/fs_imgui_image.sc.bin.h>
#include <glsl/vs_imgui_image.sc.bin.h>
#include <spirv/fs_imgui_image.sc.bin.h>
#include <spirv/vs_imgui_image.sc.bin.h>

#include <essl/fs_ocornut_imgui.sc.bin.h>
#include <essl/vs_ocornut_imgui.sc.bin.h>
#include <glsl/fs_ocornut_imgui.sc.bin.h>
#include <glsl/vs_ocornut_imgui.sc.bin.h>
#include <spirv/fs_ocornut_imgui.sc.bin.h>
#include <spirv/vs_ocornut_imgui.sc.bin.h>

#if BGFX_PLATFORM_SUPPORTS_DXBC
#include <dxbc/fs_imgui_image.sc.bin.h>
#include <dxbc/vs_imgui_image.sc.bin.h>

#include <dxbc/fs_ocornut_imgui.sc.bin.h>
#include <dxbc/vs_ocornut_imgui.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_DXIL
#include <dxil/fs_imgui_image.sc.bin.h>
#include <dxil/vs_imgui_image.sc.bin.h>

#include <dxil/fs_ocornut_imgui.sc.bin.h>
#include <dxil/vs_ocornut_imgui.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#include <metal/fs_imgui_image.sc.bin.h>
#include <metal/vs_imgui_image.sc.bin.h>

#include <metal/fs_ocornut_imgui.sc.bin.h>
#include <metal/vs_ocornut_imgui.sc.bin.h>
#endif

// Data
namespace blackboard::renderer
{

    constexpr uint32_t DEFAULT_CLEAR_COLOR = 0x000000ff;

    static uint8_t main_view_id{255};
    static bool is_init{false};
    static bgfx::TextureHandle font_texture = BGFX_INVALID_HANDLE;
    static bgfx::ProgramHandle shader_handle = BGFX_INVALID_HANDLE;
    static bgfx::ProgramHandle m_imageProgram = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle u_imageLodEnabled = BGFX_INVALID_HANDLE;
    static bgfx::UniformHandle uniform_texture = BGFX_INVALID_HANDLE;
    static bgfx::VertexLayout vertex_layout;
    static std::vector<bgfx::ViewId> free_view_ids;
    static bgfx::ViewId sub_view_id = 200;

    static uint32_t bgfx_reset_flags = BGFX_RESET_HIDPI | BGFX_RESET_VSYNC;

    void set_bgfx_reset_flags(uint32_t flags) { bgfx_reset_flags = flags; }
    uint32_t get_bgfx_reset_flags() { return bgfx_reset_flags; }

    static bgfx::ViewId allocate_view_id()
    {
        if (!free_view_ids.empty())
        {
            const bgfx::ViewId id = free_view_ids.back();
            free_view_ids.pop_back();
            return id;
        }
        return sub_view_id++;
    }

    static void free_view_id(bgfx::ViewId id) { free_view_ids.push_back(id); }

    static const bgfx::EmbeddedShader s_embeddedShaders[] = {
        BGFX_EMBEDDED_SHADER(vs_ocornut_imgui), BGFX_EMBEDDED_SHADER(fs_ocornut_imgui),
        BGFX_EMBEDDED_SHADER(vs_imgui_image), BGFX_EMBEDDED_SHADER(fs_imgui_image),

        BGFX_EMBEDDED_SHADER_END()};

    bool checkAvailTransientBuffers(uint32_t _numVertices, const bgfx::VertexLayout &_layout,
                                    uint32_t _numIndices)
    {
        return _numVertices == bgfx::getAvailTransientVertexBuffer(_numVertices, _layout) &&
               (0 == _numIndices || _numIndices == bgfx::getAvailTransientIndexBuffer(_numIndices));
    }

    enum class BgfxTextureFlags : uint32_t
    {
        Opaque = 1u << 31,
        PointSampler = 1u << 30,
        All = Opaque | PointSampler,
    };

    void *native_window_handle(ImGuiViewport *viewport, SDL_WindowID window_id)
    {
        SDL_Window *window = SDL_GetWindowFromID(window_id);
#if BX_PLATFORM_WINDOWS
        return SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                      SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif BX_PLATFORM_OSX && defined(SDL_VIDEO_DRIVER_COCOA)
        return SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                      SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#elif BX_PLATFORM_LINUX
        const char *driver = SDL_GetCurrentVideoDriver();
        if (driver && strcmp(driver, "wayland") == 0)
        {
            return SDL_GetPointerProperty(SDL_GetWindowProperties(window),
                                          SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
        }
        else
        {
            return (void *)(uintptr_t)SDL_GetNumberProperty(SDL_GetWindowProperties(window),
                                                            SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
        }
#endif
        return nullptr;
    }

    struct imgui_viewport_data
    {
        bgfx::FrameBufferHandle frameBufferHandle = BGFX_INVALID_HANDLE;
        bgfx::ViewId viewId = 0;
        uint16_t width = 0;
        uint16_t height = 0;
    };

    static void ImguiBgfxOnCreateWindow(ImGuiViewport *viewport)
    {
        auto *data = new imgui_viewport_data();
        viewport->RendererUserData = data;
        // Setup view id and size
        data->viewId = allocate_view_id();
        data->width = bx::max<uint16_t>(static_cast<uint16_t>(viewport->Size.x), 1);
        data->height = bx::max<uint16_t>(static_cast<uint16_t>(viewport->Size.y), 1);

        const auto scaledWidth = bx::max<uint16_t>(
            static_cast<uint16_t>(
                bx::ceil(static_cast<float>(data->width) * viewport->DrawData->FramebufferScale.x)),
            1);
        const auto scaledHeight = bx::max<uint16_t>(
            static_cast<uint16_t>(bx::ceil(static_cast<float>(data->height) *
                                           viewport->DrawData->FramebufferScale.y)),
            1);

        auto windowId =
            static_cast<SDL_WindowID>(reinterpret_cast<uintptr_t>(viewport->PlatformHandle));

        data->frameBufferHandle = bgfx::createFrameBuffer(native_window_handle(viewport, windowId),
                                                          scaledWidth, scaledHeight);
        // Set frame buffer
        bgfx::setViewFrameBuffer(data->viewId, data->frameBufferHandle);
        bgfx::setViewClear(data->viewId, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, DEFAULT_CLEAR_COLOR);
    }

    static void ImguiBgfxOnDestroyWindow(ImGuiViewport *viewport)
    {
        if (auto *data = static_cast<imgui_viewport_data *>(viewport->RendererUserData); data)
        {
            viewport->RendererUserData = nullptr;
            free_view_id(data->viewId);
            bgfx::destroy(data->frameBufferHandle);
            data->frameBufferHandle.idx = bgfx::kInvalidHandle;
            delete data;
        }
    }

    static void ImguiBgfxOnSetWindowSize(ImGuiViewport *viewport, ImVec2 size)
    {
        ImguiBgfxOnDestroyWindow(viewport);
        ImguiBgfxOnCreateWindow(viewport);
    }

    static void ImguiBgfxOnRenderWindow(ImGuiViewport *viewport, void *render_arg)
    {
        if (auto *data = static_cast<imgui_viewport_data *>(viewport->RendererUserData); data)
        {
            ImGui_Impl_sdl_bgfx_Render(data->viewId, viewport->DrawData,
                                       ((viewport->Flags & ImGuiViewportFlags_NoRendererClear) == 0)
                                           ? DEFAULT_CLEAR_COLOR
                                           : 0);
        }
    }

    void ImGui_Impl_sdl_bgfx_Resize(SDL_Window *window)
    {
        int drawable_width{0};
        int drawable_height{0};
        SDL_GetWindowSizeInPixels(window, &drawable_width, &drawable_height);
        ImGuiIO &io = ImGui::GetIO();
        io.DisplaySize =
            ImVec2(static_cast<float>(drawable_width), static_cast<float>(drawable_height));
        bgfx::reset(drawable_width, drawable_height, bgfx_reset_flags);
    }

    void ImGui_Impl_sdl_bgfx_UpdateTextures(ImVector<ImTextureData *> *Textures)
    {
        for (ImTextureData *texData : *Textures)
        {
            switch (texData->Status)
            {
            case ImTextureStatus_WantCreate:
            {
                TextureBgfx tex = {
                    .handle = bgfx::createTexture2D(static_cast<uint16_t>(texData->Width),
                                                    static_cast<uint16_t>(texData->Height), false,
                                                    1, bgfx::TextureFormat::BGRA8, 0),
                    .flags = IMGUI_FLAGS_ALPHA_BLEND,
                    .mip = 0,
                    .unused = 0,
                };

                bgfx::setName(tex.handle, "ImGui Font Atlas");
                bgfx::updateTexture2D(tex.handle, 0, 0, 0, 0,
                                      bx::narrowCast<uint16_t>(texData->Width),
                                      bx::narrowCast<uint16_t>(texData->Height),
                                      bgfx::copy(texData->GetPixels(), texData->GetSizeInBytes()));

                texData->SetTexID(bx::bitCast<ImTextureID>(tex));
                texData->SetStatus(ImTextureStatus_OK);
            }
            break;

            case ImTextureStatus_WantDestroy:
            {
                auto tex = bx::bitCast<TextureBgfx>(texData->GetTexID());
                bgfx::destroy(tex.handle);
                texData->SetTexID(ImTextureID_Invalid);
                texData->SetStatus(ImTextureStatus_Destroyed);
            }
            break;

            case ImTextureStatus_WantUpdates:
            {
                auto tex = bx::bitCast<TextureBgfx>(texData->GetTexID());

                for (ImTextureRect &rect : texData->Updates)
                {
                    const uint32_t bpp = texData->BytesPerPixel;
                    const bgfx::Memory *pix = bgfx::alloc(rect.h * rect.w * bpp);
                    bx::gather(pix->data, texData->GetPixelsAt(rect.x, rect.y), texData->GetPitch(),
                               rect.w * bpp, rect.h);
                    bgfx::updateTexture2D(tex.handle, 0, 0, rect.x, rect.y, rect.w, rect.h, pix);
                }
            }
            break;

            default:
                break;
            }
        }
    }

    void ImGui_Impl_sdl_bgfx_Render(const bgfx::ViewId view_id, ImDrawData *draw_data,
                                    uint32_t clear_color)
    {
        if (nullptr != draw_data->Textures)
        {
            ImGui_Impl_sdl_bgfx_UpdateTextures(draw_data->Textures);
        }

        if (ImGuiIO &io = ImGui::GetIO(); io.DisplaySize.x <= 0 || io.DisplaySize.y <= 0)
        {
            return;
        }

        if (clear_color != 0u)
        {
            bgfx::setViewClear(view_id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clear_color, 1.0f, 0);
        }
        bgfx::touch(view_id);
        bgfx::setViewName(view_id, "ImGui");
        bgfx::setViewMode(view_id, bgfx::ViewMode::Sequential);

        // (0,0) unless using multi-viewports
        const auto clip_position = draw_data->DisplayPos;
        const auto clip_size = draw_data->DisplaySize;
        // (1,1) unless using retina display which are often (2,2)
        const ImVec2 clip_scale = draw_data->FramebufferScale;
        const auto framebuffer_size = clip_size * clip_scale;

        const bgfx::Caps *caps = bgfx::getCaps();
        {
            const auto left = clip_position.x;
            const auto right = left + clip_size.x;
            const auto top = clip_position.y;
            const auto bottom = top + clip_size.y;
            float ortho[16];
            bx::mtxOrtho(ortho, left, right, bottom, top, 0.0f, 1000.0f, 0.0f,
                         bgfx::getCaps()->homogeneousDepth);
            bgfx::setViewTransform(view_id, nullptr, ortho);
            bgfx::setViewRect(view_id, 0, 0, static_cast<uint16_t>(clip_size.x * clip_scale.x),
                              static_cast<uint16_t>(clip_size.y * clip_scale.y));
        }

        // draw_data->ScaleClipRects(clipScale);
        // Render command lists
        for (int32_t ii = 0, num = draw_data->CmdLists.Size; ii < num; ++ii)
        {
            bgfx::TransientVertexBuffer tvb = {};
            bgfx::TransientIndexBuffer tib = {};

            const ImDrawList *drawList = draw_data->CmdLists[ii];
            auto numVertices = static_cast<uint32_t>(drawList->VtxBuffer.size());
            auto numIndices = static_cast<uint32_t>(drawList->IdxBuffer.size());

            if (!checkAvailTransientBuffers(numVertices, vertex_layout, numIndices))
            {
                // not enough space in transient buffer just quit drawing the rest...
                break;
            }

            bgfx::allocTransientVertexBuffer(&tvb, numVertices, vertex_layout);
            bgfx::allocTransientIndexBuffer(&tib, numIndices, sizeof(ImDrawIdx) == 4);

            auto *verts = reinterpret_cast<ImDrawVert *>(tvb.data);
            bx::memCopy(verts, drawList->VtxBuffer.begin(), numVertices * sizeof(ImDrawVert));

            auto *indices = reinterpret_cast<ImDrawIdx *>(tib.data);
            bx::memCopy(indices, drawList->IdxBuffer.begin(), numIndices * sizeof(ImDrawIdx));

            bgfx::Encoder *encoder = bgfx::begin();

            // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            for (const ImDrawCmd *cmd = drawList->CmdBuffer.begin(),
                                 *cmdEnd = drawList->CmdBuffer.end();
                 cmd != cmdEnd; ++cmd)
            {
                if (cmd->UserCallback != nullptr)
                {
                    cmd->UserCallback(drawList, cmd);
                }
                else if (0 != cmd->ElemCount)
                {
                    uint64_t state =
                        0 | BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA;

                    bgfx::TextureHandle textureHandle = BGFX_INVALID_HANDLE;
                    bgfx::ProgramHandle program = shader_handle;

                    const ImTextureID texId = cmd->GetTexID();

                    if (ImTextureID_Invalid != texId)
                    {
                        auto tex = bx::bitCast<TextureBgfx>(texId);

                        state |= 0 != (IMGUI_FLAGS_ALPHA_BLEND & tex.flags)
                                     ? BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                             BGFX_STATE_BLEND_INV_SRC_ALPHA)
                                     : BGFX_STATE_NONE;
                        textureHandle = tex.handle;

                        if (0 != tex.mip)
                        {
                            const float lodEnabled[4] = {static_cast<float>(tex.mip), 1.0f, 0.0f,
                                                         0.0f};
                            bgfx::setUniform(u_imageLodEnabled, lodEnabled);
                            program = m_imageProgram;
                        }
                    }
                    else
                    {
                        state |= BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA,
                                                       BGFX_STATE_BLEND_INV_SRC_ALPHA);
                    }

                    // Project scissor/clipping rectangles into framebuffer space
                    ImVec4 clipRect;
                    clipRect.x = (cmd->ClipRect.x - clip_position.x) * clip_scale.x;
                    clipRect.y = (cmd->ClipRect.y - clip_position.y) * clip_scale.y;
                    clipRect.z = (cmd->ClipRect.z - clip_position.x) * clip_scale.x;
                    clipRect.w = (cmd->ClipRect.w - clip_position.y) * clip_scale.y;

                    if (clipRect.x < framebuffer_size.x && clipRect.y < framebuffer_size.y &&
                        clipRect.z >= 0.0f && clipRect.w >= 0.0f)
                    {
                        const uint16_t clipX = static_cast<uint16_t>(
                            bx::max<float>(cmd->ClipRect.x - clip_position.x, 0.0f));
                        const uint16_t clipY = static_cast<uint16_t>(
                            bx::max<float>(cmd->ClipRect.y - clip_position.y, 0.0f));
                        const uint16_t width = static_cast<uint16_t>(bx::min<float>(
                            cmd->ClipRect.z - clip_position.x - static_cast<float>(clipX),
                            65535.0f));
                        const uint16_t height = static_cast<uint16_t>(bx::min<float>(
                            cmd->ClipRect.w - clip_position.y - static_cast<float>(clipY),
                            65535.0f));
                        encoder->setScissor(
                            static_cast<uint16_t>(static_cast<float>(clipX) * clip_scale.x),
                            static_cast<uint16_t>(static_cast<float>(clipY) * clip_scale.y),
                            static_cast<uint16_t>(static_cast<float>(width) * clip_scale.x),
                            static_cast<uint16_t>(static_cast<float>(height) * clip_scale.y));

                        encoder->setState(state);
                        encoder->setTexture(0, uniform_texture, textureHandle);
                        encoder->setVertexBuffer(0, &tvb, cmd->VtxOffset, numVertices);
                        encoder->setIndexBuffer(&tib, cmd->IdxOffset, cmd->ElemCount);
                        encoder->submit(view_id, program);
                    }
                }
            }
            // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)

            bgfx::end(encoder);
        }
    }

    void ImGui_Implbgfx_CreateDeviceObjects()
    {
        const auto type = bgfx::getRendererType();
        shader_handle = bgfx::createProgram(
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_ocornut_imgui"),
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_ocornut_imgui"), true);

        u_imageLodEnabled = bgfx::createUniform("u_imageLodEnabled", bgfx::UniformType::Vec4);
        m_imageProgram = bgfx::createProgram(
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_imgui_image"),
            bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_imgui_image"), true);

        vertex_layout.begin()
            .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
            .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
            .end();

        uniform_texture = bgfx::createUniform("s_tex", bgfx::UniformType::Sampler);

        is_init = true;
    }

    void ImGui_Implbgfx_InvalidateDeviceObjects()
    {
        for (ImTextureData *texData : ImGui::GetPlatformIO().Textures)
        {
            if (1 == texData->RefCount)
            {
                auto tex = bx::bitCast<TextureBgfx>(texData->GetTexID());
                bgfx::destroy(tex.handle);
                texData->SetTexID(ImTextureID_Invalid);
                texData->SetStatus(ImTextureStatus_Destroyed);
            }
        }

        if (bgfx::isValid(shader_handle))
        {
            bgfx::destroy(shader_handle);
            shader_handle.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(m_imageProgram))
        {
            bgfx::destroy(m_imageProgram);
            m_imageProgram.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(u_imageLodEnabled))
        {
            bgfx::destroy(u_imageLodEnabled);
            u_imageLodEnabled.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(font_texture))
        {
            bgfx::destroy(font_texture);
            ImGui::GetIO().Fonts->TexRef = {};
            font_texture.idx = bgfx::kInvalidHandle;
        }

        if (bgfx::isValid(uniform_texture))
        {
            bgfx::destroy(uniform_texture);
            uniform_texture.idx = bgfx::kInvalidHandle;
        }
    }

    void ImGui_Impl_sdl_bgfx_Init(int view)
    {
        main_view_id = static_cast<uint8_t>(view & 0xff);

        ImGuiIO &io = ImGui::GetIO();
        io.BackendFlags |=
            0 | ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

        ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
        platform_io.Renderer_CreateWindow = ImguiBgfxOnCreateWindow;
        platform_io.Renderer_DestroyWindow = ImguiBgfxOnDestroyWindow;
        platform_io.Renderer_SetWindowSize = ImguiBgfxOnSetWindowSize;
        platform_io.Renderer_RenderWindow = ImguiBgfxOnRenderWindow;
    }

    void ImGui_Impl_sdl_bgfx_Shutdown() { ImGui_Implbgfx_InvalidateDeviceObjects(); }

    void ImGui_Impl_sdl_bgfx_NewFrame()
    {
        if (!is_init)
        {
            ImGui_Implbgfx_CreateDeviceObjects();
        }
    }

} // namespace blackboard::renderer
