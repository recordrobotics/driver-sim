#include <imgui/imgui.h>
#include <blackboard_app/gui.h>

#include "components.h"

using blackboard::gui::string_hex_to_rgba_float;
using blackboard::gui::string_hex_to_rgba_u32;

void ui::DrawCenteredText(const char *text, float yOffset)
{
    ImVec2 winSize = ImGui::GetWindowSize();
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImGui::SetCursorPosX((winSize.x - textSize.x) * 0.5f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
    ImGui::TextUnformatted(text);
}

void ui::DrawProgress(const std::string_view &label, float value, bool isError)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImVec2 winSize = ImGui::GetWindowSize();
    float width = 350.0f * globalScale;

    ImGui::SetCursorPosX((winSize.x - width) * 0.5f);

    char buf[128];
    snprintf(buf, sizeof(buf), "%.*s (%.0f%%)", static_cast<int>(label.size()), label.data(), value * 100.0f);

    ImGui::PushFont(nullptr, 16.0f);
    ImGui::TextColored(string_hex_to_rgba_float("#A7A7A7ff"), "%s", buf);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 8 * globalScale));

    ImGui::SetCursorPosX((winSize.x - width) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, string_hex_to_rgba_float("#2D2A2Aff"));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, isError ? string_hex_to_rgba_float("#c83c3c") : string_hex_to_rgba_float("#42A749ff"));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f * globalScale);
    ImGui::ProgressBar(value, ImVec2(width, 7 * globalScale), "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Dummy(ImVec2(0, 18 * globalScale));
}

void ui::SplitToggleButtonGroup(std::list<ToggleButton> buttons)
{
    if (buttons.empty())
    {
        return;
    }

    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;
    ImVec2 winSize = ImGui::GetWindowSize();

    const ImVec2 framePadding(16.0f * globalScale, 7.0f * globalScale);
    const float frameRounding = 6.0f * globalScale;
    const float borderThickness = 1.0f * globalScale;

    ImU32 off_normal = string_hex_to_rgba_u32("#ffffff00");
    ImU32 off_hovered = string_hex_to_rgba_u32("#ffffff0a");
    ImU32 off_active = string_hex_to_rgba_u32("#ffffff28");
    ImU32 on_normal = string_hex_to_rgba_u32("#38903Eff");
    ImU32 on_hovered = string_hex_to_rgba_u32("#449c4aff");
    ImU32 on_active = string_hex_to_rgba_u32("#44b24bff");
    ImU32 textColor = string_hex_to_rgba_u32("#FFFFFFff");
    ImU32 borderColor = string_hex_to_rgba_u32("#3E3E3Eff");

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    ImGui::PushFont(nullptr, 16.0f);

    float totalWidth = 0;
    for (auto &button : buttons)
    {
        totalWidth += ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;
    }

    const float segmentHeight = ImGui::GetTextLineHeight() + framePadding.y * 2.0f;

    ImGui::SetCursorPosX((winSize.x - totalWidth) / 2);
    const ImVec2 groupMin = ImGui::GetCursorScreenPos();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const size_t buttonCount = buttons.size();
    size_t index = 0;

    for (auto &button : buttons)
    {
        const float segmentWidth = ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;

        ImGui::PushID(static_cast<int>(index));
        const ImVec2 segmentPos = ImGui::GetCursorScreenPos();
        const ImVec2 segmentSize(segmentWidth, segmentHeight);

        const bool pressed = ImGui::InvisibleButton("##segment", segmentSize);
        if (pressed)
        {
            *button.state = !(*button.state);
        }

        const bool hovered = ImGui::IsItemHovered();
        const bool active = ImGui::IsItemActive();

        if (hovered)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }

        const ImU32 fillColor = *button.state
                                    ? (active ? on_active : (hovered ? on_hovered : on_normal))
                                    : (active ? off_active : (hovered ? off_hovered : off_normal));

        ImDrawFlags cornerFlags = ImDrawFlags_RoundCornersNone;
        if (buttonCount == 1)
        {
            cornerFlags = ImDrawFlags_RoundCornersAll;
        }
        else if (index == 0)
        {
            cornerFlags = ImDrawFlags_RoundCornersLeft;
        }
        else if (index + 1 == buttonCount)
        {
            cornerFlags = ImDrawFlags_RoundCornersRight;
        }

        drawList->AddRectFilled(
            segmentPos,
            ImVec2(segmentPos.x + segmentSize.x, segmentPos.y + segmentSize.y),
            fillColor,
            frameRounding,
            cornerFlags);

        const ImVec2 textSize = ImGui::CalcTextSize(button.label);
        const ImVec2 textPos(
            segmentPos.x + (segmentSize.x - textSize.x) * 0.5f,
            segmentPos.y + (segmentSize.y - textSize.y) * 0.5f);

        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textColor, button.label);

        ImGui::PopID();

        ++index;
        if (index < buttonCount)
        {
            ImGui::SameLine(0.0f, 0.0f);
        }
    }

    const ImVec2 groupMax(groupMin.x + totalWidth, groupMin.y + segmentHeight);
    drawList->AddRect(
        groupMin,
        groupMax,
        borderColor,
        frameRounding,
        ImDrawFlags_RoundCornersAll,
        borderThickness);

    float dividerX = groupMin.x;
    size_t dividerIndex = 0;
    for (auto &button : buttons)
    {
        dividerX += ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;
        ++dividerIndex;
        if (dividerIndex < buttonCount)
        {
            drawList->AddLine(
                ImVec2(dividerX, groupMin.y),
                ImVec2(dividerX, groupMax.y),
                borderColor,
                borderThickness);
        }
    }

    ImGui::PopFont();

    ImGui::PopStyleVar();
}

bool ui::UnderlineTextButton(const char *text)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::PushFont(nullptr, 12.0f);
    ImVec2 winSize = ImGui::GetWindowSize();
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    ImGui::SetCursorPosX((winSize.x - textSize.x) / 2);

    ImGui::PushID(text);
    bool pressed = ImGui::InvisibleButton("##underline_btn", textSize);
    ImGui::PopID();

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImU32 textColor = active ? string_hex_to_rgba_u32("#5cbd62ff") : hovered ? string_hex_to_rgba_u32("#47a54dff")
                                                                             : string_hex_to_rgba_u32("#38903Eff");

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImGui::GetWindowDrawList()->AddText(min, textColor, text);

    float thickness = 1.5f * globalScale;
    float offset = 2.0f * globalScale;

    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(min.x, max.y + offset),
        ImVec2(max.x, max.y + offset),
        textColor,
        thickness);

    ImGui::PopFont();

    return pressed;
}

bool ui::CircularButton(const char *id, float radius)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    bool pressed = ImGui::InvisibleButton(id, ImVec2(radius * 2, radius * 2));

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImU32 color = active ? string_hex_to_rgba_u32("#71d478ff") : hovered ? string_hex_to_rgba_u32("#5dbb64ff")
                                                                         : string_hex_to_rgba_u32("#42A749ff");

    draw->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius), radius, color);

    // Arrow rendered as 3 stroked segments with round endcaps.
    const ImVec2 center(pos.x + radius, pos.y + radius);
    const float arrowThickness = radius * 0.13f;
    const float capRadius = arrowThickness * 0.5f;

    const ImVec2 shaftStart(center.x - radius * 0.45f, center.y);
    const ImVec2 shaftEnd(center.x + radius * 0.45f, center.y);
    const ImVec2 arrowTip(center.x + radius * 0.45f, center.y);
    const ImVec2 headTop(center.x, arrowTip.y - radius * 0.45f);
    const ImVec2 headBottom(center.x, arrowTip.y + radius * 0.45f);

    auto drawRoundedSegment = [&](const ImVec2 &a, const ImVec2 &b)
    {
        draw->AddLine(a, b, IM_COL32_WHITE, arrowThickness);
        draw->AddCircleFilled(a, capRadius, IM_COL32_WHITE);
        draw->AddCircleFilled(b, capRadius, IM_COL32_WHITE);
    };

    drawRoundedSegment(shaftStart, shaftEnd);
    drawRoundedSegment(headTop, arrowTip);
    drawRoundedSegment(headBottom, arrowTip);

    return pressed;
}
