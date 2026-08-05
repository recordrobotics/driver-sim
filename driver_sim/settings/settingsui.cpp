#include "settingsui.h"

#include <blackboard_app/gui.h>

#include <logo.png.h>

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;

namespace settings
{
    ImTexture logo = {};
};

void settings::init(ImTexture &logo)
{
    settings::logo = logo;
}

void settings::cleanup()
{
}

void DrawVerticallyCenteredText(const char *text, float heightAvailable)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (heightAvailable - textSize.y) * 0.5f);
    ImGui::TextUnformatted(text);
}

void settings::draw(ImFont *font, ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    const ImVec2 p = ImVec2(40.0f * globalScale, 40.0f * globalScale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, p);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::SetNextWindowPos(viewportPos);
    ImGui::SetNextWindowSize(viewportSize);
    ImGui::SetNextWindowViewport(viewportId);

    if (ImGui::Begin("Settings", nullptr,
                     ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_NoBringToFrontOnFocus |
                         ImGuiWindowFlags_NoDocking))
    {
        const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);

        // Logo
        if (logo.id)
        {
            ImGui::Image(logo.id, logoSize);
        }
        else
        {
            ImGui::Dummy(logoSize);
        }

        ImGui::Dummy(ImVec2(0, 6 * globalScale));

        ImGui::PushFont(nullptr, 35.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8F8686ff"));
        DrawVerticallyCenteredText("Driver Sim", logoSize.y);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
}