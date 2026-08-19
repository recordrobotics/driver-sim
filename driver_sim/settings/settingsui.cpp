#include "settingsui.h"

#include "../ui/components.h"
#include <blackboard_app/gui.h>

#include <logo.png.h>

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;
using blackboard::gui::string_hex_to_rgba_u32;

namespace settings
{
    ImTexture logo = {};
};

void settings::init(ImTexture &logo) { settings::logo = logo; }

void settings::cleanup() {}

void DrawVerticallyCenteredText(const char *text, float heightAvailable)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((heightAvailable - textSize.y) * 0.5f));
    ImGui::TextUnformatted(text);
}

bool DrawLinkText(const char *label, const char *url = nullptr)
{
    if (url == nullptr)
    {
        url = label;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::CalcTextSize(label);
    ImDrawList *draw = ImGui::GetWindowDrawList();

    bool pressed = ImGui::InvisibleButton(label, size);

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImU32 col = string_hex_to_rgba_u32("#6C74FAFF");

    if (active)
    {
        col = string_hex_to_rgba_u32("#767ce3FF");
    }
    else if (hovered)
    {
        col = string_hex_to_rgba_u32("#5b63f0FF");
    }

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (pressed)
    {
        ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
        if (platform_io.Platform_OpenInShellFn != nullptr)
        {
            platform_io.Platform_OpenInShellFn(ImGui::GetCurrentContext(), url);
        }
    }

    draw->AddText(pos, col, label);

    return pressed;
}

void drawAboutPanel(ImFont *font)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::Dummy(ImVec2(0, 50.0f * globalScale));

    ImGui::PushFont(nullptr, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#D3D3D3FF"));
    ImGui::TextUnformatted("About Driver Sim");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 5.0f * globalScale));

    ImGui::PushFont(nullptr, 10.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A2A2A2FF"));
    ImGui::TextUnformatted("Version 1.0.0 (2026) Build 2026.7.16+ (Packaged) dc88a1a d1d8cba");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::PopStyleColor();

    DrawLinkText("https://github.com/recordrobotics/2026-robot");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    DrawLinkText("https://github.com/recordrobotics/driver-sim");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A2A2A2FF"));

    ImGui::TextUnformatted("Stored Assets");
    ImGui::Dummy(ImVec2(0, 6.0f * globalScale));

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "jdk:8c7cfff78a55c56ebaf470ed6a89c6466b47d8274bdabdda997d7507c20325c5 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "elastic:6581e66eb237f9d615afb94077d89a03e2cdd7ce2d57f11c8cc5153821493ad7 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "field:0f2abde864422367dd1bc3254da23b36a3d82eb727d5dac0a0f2231bdc397e31 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "robot:b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "jni:0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "code:7998021ca2a0f0d8867173cd7fcf8f4b15fb36d011d98df55b00bebb76732878 (packaged)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "discord_sdk:2a7c8b043ca04a14a10c64b4f1116fe2a93bb6f6f4f0b4784c0ca1fc06ca832e (remote)");

    ImGui::Dummy(ImVec2(0, 6.0f * globalScale));

    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#858585FF"));
    ImGui::TextUnformatted("Made by Record Robotics");
    ImGui::PopStyleColor();

    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 10.0f * globalScale));
}

void drawHeader(const char *text)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::Dummy(ImVec2(0, 60.0f * globalScale));
    ImGui::SameLine();
    ImGui::PushFont(nullptr, 24.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#928E8Eff"));
    DrawVerticallyCenteredText(text, 60.0f * globalScale);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void drawSettingOption(const char *id, const char *name, const char *description)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::PushFont(nullptr, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8A8A8AFF"));
    ImGui::TextUnformatted(id);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));

    ImGui::PushFont(nullptr, 20.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#D1D1D1FF"));
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 2.0f * globalScale));

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 370.0f * globalScale);
    ImGui::PushFont(nullptr, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A7A7A7FF"));
    ImGui::TextWrapped(description);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10.0f * globalScale, 0));

    ImGui::NextColumn();

    ImGui::PushFont(nullptr, 11.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#F0F25FFF"));
    ui::TextAlignedWrapped(ui::TextAlign::Right,
                           "This value is different from the default of 'true'.");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#EC9658FF"));
    ui::TextAlignedWrapped(ui::TextAlign::Right, "Reset to default");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Columns(1);
}

void settings::draw(ImFont *font, ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    const ImVec2 padding = ImVec2(50.0f * globalScale, 40.0f * globalScale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::SetNextWindowPos(viewportPos);
    ImGui::SetNextWindowSize(viewportSize);
    ImGui::SetNextWindowViewport(viewportId);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Settings", nullptr, flags))
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0f * globalScale);
        const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);

        // Logo
        if (logo.id != 0u)
        {
            ImGui::Image(logo.id, logoSize);
        }
        else
        {
            ImGui::Dummy(logoSize);
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(28.0f * globalScale, 0));

        ImGui::SameLine();
        ImGui::PushFont(nullptr, 35.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8F8686ff"));
        DrawVerticallyCenteredText("Settings", logoSize.y);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 20.0f * globalScale));

        drawHeader("General");

        drawSettingOption(
            "showMainMenu", "Show main menu",
            "Determines whether the main menu should show when Driver Sim is started. When false "
            "Driver Sim opens directly to the 3D field view.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("showExitWarning", "Show exit warning",
                          "Determines whether to show the warning confirmation message when "
                          "leaving the 3D field view and going back to the main menu.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("launchRobotCode", "Launch robot code",
                          "Whether to launch the robot code when opening the 3D field view. "
                          "Disable if using a separate instance of the robot code, for example "
                          "when working as a developer on the code.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawHeader("Game Specific");
        drawHeader("Simulation");
        drawHeader("Graphics");

        drawAboutPanel(font);
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
}