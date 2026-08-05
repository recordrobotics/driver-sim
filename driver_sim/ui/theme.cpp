#define IMGUI_DEFINE_MATH_OPERATORS

#include <blackboard_app/gui.h>
#include <imgui/imgui.h>

#include "theme.h"

using blackboard::gui::string_hex_to_rgba_float;

void ui::set_theme()
{
    ImGui::StyleColorsDark();

    static ImVec4 background{string_hex_to_rgba_float("#1E1E1Eff")};
    static auto selection{string_hex_to_rgba_float("#445a46ff")};
    static auto foreground{string_hex_to_rgba_float("#BBBBBBff")};
    static auto comment{string_hex_to_rgba_float("#38903Eff")};
    static auto cyan{string_hex_to_rgba_float("#8be9fdff")};
    static auto green{string_hex_to_rgba_float("#50fa7bff")};
    static auto orange{string_hex_to_rgba_float("#ffb86cff")};
    static auto pink{string_hex_to_rgba_float("#ff79c6ff")};
    static auto purple{string_hex_to_rgba_float("#bd93f9ff")};
    static auto red{string_hex_to_rgba_float("#ff5555ff")};
    static auto yellow{string_hex_to_rgba_float("#f1fa8cff")};

    const auto dark_alpha_selection{selection * ImVec4{1.0f, 1.0f, 1.0f, 0.5f}};
    const auto dark_alpha_green{green * ImVec4{1.0f, 1.0f, 1.0f, 0.3f}};
    const auto darker_background{background * ImVec4{0.15f, 0.15f, 0.15f, 1.0f}};
    const auto dark_alpha_red{red * ImVec4{1.0f, 1.0f, 1.0f, 0.10f}};

    auto &colors{ImGui::GetStyle().Colors};

    const auto IconColour{ImVec4{0.718, 0.62f, 0.86f, 1.00f}};
    colors[ImGuiCol_Text] = foreground;
    colors[ImGuiCol_TextSelectedBg] = comment;
    colors[ImGuiCol_TextDisabled] = string_hex_to_rgba_float("#666666ff");

    colors[ImGuiCol_WindowBg] = background;
    colors[ImGuiCol_ChildBg] = background;

    colors[ImGuiCol_PopupBg] = background;
    colors[ImGuiCol_Border] = dark_alpha_green;
    colors[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
    colors[ImGuiCol_FrameBg] = selection;
    colors[ImGuiCol_FrameBgHovered] = selection * ImVec4{1.1f, 1.1f, 1.1f, 1.0f};
    colors[ImGuiCol_FrameBgActive] = selection * ImVec4{1.2f, 1.2f, 1.2f, 1.0f};

    colors[ImGuiCol_TitleBg] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
    colors[ImGuiCol_TitleBgActive] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
    colors[ImGuiCol_TitleBgCollapsed] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
    colors[ImGuiCol_MenuBarBg] = selection;

    colors[ImGuiCol_ScrollbarBg] = ImVec4{0.02f, 0.02f, 0.02f, 0.39f};
    colors[ImGuiCol_ScrollbarGrab] = dark_alpha_selection;
    colors[ImGuiCol_ScrollbarGrabActive] = dark_alpha_selection * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
    colors[ImGuiCol_ScrollbarGrabHovered] = dark_alpha_selection * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};

    colors[ImGuiCol_CheckMark] = comment;
    colors[ImGuiCol_SliderGrab] = comment;
    colors[ImGuiCol_SliderGrabActive] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
    colors[ImGuiCol_Button] = comment;
    colors[ImGuiCol_ButtonHovered] = comment * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
    colors[ImGuiCol_ButtonActive] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

    colors[ImGuiCol_Separator] = selection;
    colors[ImGuiCol_SeparatorHovered] = selection;
    colors[ImGuiCol_SeparatorActive] = selection;

    colors[ImGuiCol_ResizeGrip] = dark_alpha_green;
    colors[ImGuiCol_ResizeGripHovered] = dark_alpha_green * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
    colors[ImGuiCol_ResizeGripActive] = dark_alpha_green * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

    colors[ImGuiCol_PlotLines] = yellow;
    colors[ImGuiCol_PlotLinesHovered] = yellow * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
    colors[ImGuiCol_PlotHistogram] = yellow;
    colors[ImGuiCol_PlotHistogramHovered] = yellow * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};

    colors[ImGuiCol_DragDropTarget] = red;

    colors[ImGuiCol_NavCursor] = red;
    colors[ImGuiCol_NavWindowingHighlight] = comment;
    colors[ImGuiCol_NavWindowingDimBg] = red;
    colors[ImGuiCol_ModalWindowDimBg] = dark_alpha_red;

    colors[ImGuiCol_Header] = dark_alpha_selection;
    colors[ImGuiCol_HeaderHovered] = dark_alpha_selection * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
    colors[ImGuiCol_HeaderActive] = dark_alpha_selection * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

    colors[ImGuiCol_Tab] = comment;
    colors[ImGuiCol_TabHovered] = comment * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
    colors[ImGuiCol_TabSelected] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
    colors[ImGuiCol_TabDimmed] = comment * ImVec4{0.5f, 0.5f, 0.5f, 0.5f};
    colors[ImGuiCol_TabDimmedSelected] = comment * ImVec4{0.5f, 0.5f, 0.5f, 0.5f};

    colors[ImGuiCol_DockingEmptyBg] = darker_background;
    colors[ImGuiCol_DockingPreview] = dark_alpha_green;

    colors[ImGuiCol_TableHeaderBg] = comment;
    colors[ImGuiCol_TableBorderLight] = dark_alpha_green;
    colors[ImGuiCol_TableBorderStrong] = dark_alpha_green;

    auto &style{ImGui::GetStyle()};
    style.FramePadding = {2.0f, 2.0f};
    style.CellPadding = {2.0f, 2.0f};
    style.TabBorderSize = 1.0f;
    style.TabRounding = 1.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.WindowRounding = 2.0f;
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
}