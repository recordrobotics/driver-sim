#include <algorithm>
#include <blackboard_app/gui.h>
#include <cmath>
#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <unordered_map>

#include "components.h"

using blackboard::gui::string_hex_to_rgba_float;
using blackboard::gui::string_hex_to_rgba_u32;
using blackboard::gui::u32_multiply_alpha;

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-vararg)

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
    snprintf(buf, sizeof(buf), "%.*s (%.0f%%)", static_cast<int>(label.size()), label.data(),
             value * 100.0f);

    ImGui::PushFont(nullptr, 16.0f);
    ImGui::TextColored(string_hex_to_rgba_float("#A7A7A7ff"), "%s", buf);
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 8 * globalScale));

    ImGui::SetCursorPosX((winSize.x - width) * 0.5f);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, string_hex_to_rgba_float("#2D2A2Aff"));
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, isError ? string_hex_to_rgba_float("#c83c3c")
                                                          : string_hex_to_rgba_float("#42A749ff"));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f * globalScale);
    ImGui::ProgressBar(value, ImVec2(width, 7 * globalScale), "");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    ImGui::Dummy(ImVec2(0, 18 * globalScale));
}

void ui::SplitToggleButtonGroup(const std::list<ToggleButton> &buttons)
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
    for (const auto &button : buttons)
    {
        totalWidth += ImGui::CalcTextSize(button.label).x + (framePadding.x * 2.0f);
    }

    const float segmentHeight = ImGui::GetTextLineHeight() + (framePadding.y * 2.0f);

    ImGui::SetCursorPosX((winSize.x - totalWidth) / 2);
    const ImVec2 groupMin = ImGui::GetCursorScreenPos();

    ImDrawList *drawList = ImGui::GetWindowDrawList();
    const size_t buttonCount = buttons.size();
    size_t index = 0;

    for (const auto &button : buttons)
    {
        const float segmentWidth = ImGui::CalcTextSize(button.label).x + (framePadding.x * 2.0f);

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

        drawList->AddRectFilled(segmentPos,
                                ImVec2(segmentPos.x + segmentSize.x, segmentPos.y + segmentSize.y),
                                fillColor, frameRounding, cornerFlags);

        const ImVec2 textSize = ImGui::CalcTextSize(button.label);
        const ImVec2 textPos(segmentPos.x + ((segmentSize.x - textSize.x) * 0.5f),
                             segmentPos.y + ((segmentSize.y - textSize.y) * 0.5f));

        drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textColor, button.label);

        ImGui::PopID();

        ++index;
        if (index < buttonCount)
        {
            ImGui::SameLine(0.0f, 0.0f);
        }
    }

    const ImVec2 groupMax(groupMin.x + totalWidth, groupMin.y + segmentHeight);
    drawList->AddRect(groupMin, groupMax, borderColor, frameRounding, borderThickness,
                      ImDrawFlags_RoundCornersAll);

    float dividerX = groupMin.x;
    size_t dividerIndex = 0;
    for (const auto &button : buttons)
    {
        dividerX += ImGui::CalcTextSize(button.label).x + (framePadding.x * 2.0f);
        ++dividerIndex;
        if (dividerIndex < buttonCount)
        {
            drawList->AddLine(ImVec2(dividerX, groupMin.y), ImVec2(dividerX, groupMax.y),
                              borderColor, borderThickness);
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

    ImU32 textColor = active    ? string_hex_to_rgba_u32("#5cbd62ff")
                      : hovered ? string_hex_to_rgba_u32("#47a54dff")
                                : string_hex_to_rgba_u32("#38903Eff");

    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();

    ImGui::GetWindowDrawList()->AddText(min, textColor, text);

    float thickness = 1.5f * globalScale;
    float offset = 2.0f * globalScale;

    ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x, max.y + offset),
                                        ImVec2(max.x, max.y + offset), textColor, thickness);

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

    ImU32 color = active    ? string_hex_to_rgba_u32("#71d478ff")
                  : hovered ? string_hex_to_rgba_u32("#5dbb64ff")
                            : string_hex_to_rgba_u32("#42A749ff");

    draw->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius), radius, color);

    // Arrow rendered as 3 stroked segments with round endcaps.
    const ImVec2 center(pos.x + radius, pos.y + radius);
    const float arrowThickness = radius * 0.13f;
    const float capRadius = arrowThickness * 0.5f;

    const ImVec2 shaftStart(center.x - (radius * 0.45f), center.y);
    const ImVec2 shaftEnd(center.x + (radius * 0.45f), center.y);
    const ImVec2 arrowTip(center.x + (radius * 0.45f), center.y);
    const ImVec2 headTop(center.x, arrowTip.y - (radius * 0.45f));
    const ImVec2 headBottom(center.x, arrowTip.y + (radius * 0.45f));

    auto drawRoundedSegment = [&](const ImVec2 &start, const ImVec2 &end)
    {
        draw->AddLine(start, end, IM_COL32_WHITE, arrowThickness);
        draw->AddCircleFilled(start, capRadius, IM_COL32_WHITE);
        draw->AddCircleFilled(end, capRadius, IM_COL32_WHITE);
    };

    drawRoundedSegment(shaftStart, shaftEnd);
    drawRoundedSegment(headTop, arrowTip);
    drawRoundedSegment(headBottom, arrowTip);

    return pressed;
}

bool ui::IconButton(ImFont *font, const char *id, std::string_view text, ImTextureID icon,
                    float size, float borderSize, float rounding, float fontSize, float textOffset,
                    float opacity, bool inverted)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    bool pressed = ImGui::InvisibleButton(id, ImVec2(size, size));

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImU32 fill{};
    ImU32 border = string_hex_to_rgba_u32("#A8A8A8C7");
    ImU32 iconColor{};
    if (inverted)
    {
        fill = active    ? string_hex_to_rgba_u32("#D0D0D0ff")
               : hovered ? string_hex_to_rgba_u32("#E0E0E0ff")
                         : string_hex_to_rgba_u32("#ffffffff");
        iconColor = string_hex_to_rgba_u32("#2189DEff");
    }
    else
    {
        fill = active    ? string_hex_to_rgba_u32("#ffffff3B")
               : hovered ? string_hex_to_rgba_u32("#ffffff31")
                         : string_hex_to_rgba_u32("#ffffff00");
        iconColor = string_hex_to_rgba_u32("#ffffffff");
    }

    fill = u32_multiply_alpha(fill, opacity);
    border = u32_multiply_alpha(border, opacity);
    iconColor = u32_multiply_alpha(iconColor, opacity);

    draw->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), fill, rounding);

    draw->AddRect(
        ImVec2(pos.x + (borderSize / 2.0f) - 1, pos.y + (borderSize / 2.0f) - 1),
        ImVec2(pos.x + size - (borderSize / 2.0f) + 1, pos.y + size - (borderSize / 2.0f) + 1),
        border, rounding, borderSize);

    const ImVec2 center(pos.x + (size * 0.5f), pos.y + (size * 0.5f));
    const float imageSize = size * 0.5f;

    draw->AddImage(icon, ImVec2(center.x - (imageSize * 0.5f), center.y - (imageSize * 0.5f)),
                   ImVec2(center.x + (imageSize * 0.5f), center.y + (imageSize * 0.5f)),
                   ImVec2(0, 0), ImVec2(1, 1), iconColor);

    ImVec2 textSize =
        font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text.data(), text.data() + text.size());

    draw->AddText(font, fontSize, ImVec2(center.x - (textSize.x / 2.0f), pos.y + size + textOffset),
                  u32_multiply_alpha(string_hex_to_rgba_u32("#FFFFFFFF"), opacity), text.data(),
                  text.data() + text.size());

    return pressed;
}

namespace ui
{
    inline float AddAlignedWrappedText(ImDrawList *draw, ImFont *font, float fontSize,
                                       const ImVec2 &pos, float wrapWidth, ImU32 color,
                                       TextAlign align, const char *text,
                                       const char *textEnd = nullptr)
    {
        if (text == nullptr)
        {
            return 0.0f;
        }

        if (textEnd == nullptr)
        {
            textEnd = text + std::strlen(text);
        }

        float lineY = pos.y;
        const float lineHeight = fontSize;

        while (text < textEnd)
        {
            const char *lineEnd = font->CalcWordWrapPosition(fontSize, text, textEnd, wrapWidth);

            for (const char *chr = text; chr < lineEnd; ++chr)
            {
                if (*chr == '\n')
                {
                    lineEnd = chr;
                    break;
                }
            }

            if (lineEnd == text)
            {
                if (*text == '\n')
                {
                    ++text;
                    lineY += lineHeight;
                    continue;
                }

                unsigned int utfChar{};
                lineEnd = text + ImTextCharFromUtf8(&utfChar, text, textEnd);
            }

            const float lineWidth = font->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text, lineEnd).x;

            switch (align)
            {
            case TextAlign::Left:
                draw->AddText(font, fontSize, ImVec2(pos.x, lineY), color, text, lineEnd);
                break;
            case TextAlign::Center:
                draw->AddText(font, fontSize,
                              ImVec2(pos.x + ((wrapWidth - lineWidth) * 0.5f), lineY), color, text,
                              lineEnd);
                break;
            case TextAlign::Right:
                draw->AddText(font, fontSize, ImVec2(pos.x + (wrapWidth - lineWidth), lineY), color,
                              text, lineEnd);
                break;
            }

            lineY += lineHeight;

            text = lineEnd;

            if (text < textEnd && *text == '\n')
            {
                ++text;
            }

            while (text < textEnd && ImCharIsBlankA(*text))
            {
                ++text;
            }
        }

        return lineY - pos.y;
    }
}; // namespace ui

void ui::TextAlignedWrapped(TextAlign align, const char *text)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();
    ImFont *font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();
    ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

    float height = AddAlignedWrappedText(draw, font, fontSize, pos,
                                         ImGui::GetContentRegionAvail().x, textColor, align, text);

    ImGui::Dummy(ImVec2(0, height));
}

void ui::DrawVerticallyCenteredText(const char *text, float heightAvailable)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((heightAvailable - textSize.y) * 0.5f));
    ImGui::TextUnformatted(text);
}

bool ui::DrawLinkText(const char *label, ui::TextAlign align, ui::LinkTextOptions options,
                      const char *id, const char *url)
{
    if (id == nullptr)
    {
        id = label;
    }

    if (url == nullptr)
    {
        url = label;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImFont *font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    ImVec2 size = ImGui::CalcTextSize(label, nullptr, false, ImGui::GetContentRegionAvail().x);

    switch (align)
    {
    case TextAlign::Left:
        break;
    case TextAlign::Center:
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                             ((ImGui::GetContentRegionAvail().x - size.x) * 0.5f));
        break;
    case TextAlign::Right:
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - size.x);
        break;
    }

    bool pressed = ImGui::InvisibleButton(id, size);

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImU32 col = options.color;

    if (active)
    {
        col = options.activeColor;
    }
    else if (hovered)
    {
        col = options.hoverColor;
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

    float height = AddAlignedWrappedText(draw, font, fontSize, pos,
                                         ImGui::GetContentRegionAvail().x, col, align, label);

    if (options.underline)
    {
        switch (align)
        {
        case TextAlign::Left:
            break;
        case TextAlign::Center:
            pos.x += (ImGui::GetContentRegionAvail().x - size.x) * 0.5f;
            break;
        case TextAlign::Right:
            pos.x += ImGui::GetContentRegionAvail().x - size.x;
            break;
        }

        draw->AddLine(ImVec2(pos.x, pos.y + height), ImVec2(pos.x + size.x, pos.y + height), col);
    }

    return pressed;
}

bool ui::ChoiceButton(ImFont *font, const char *id, std::string_view name,
                      std::string_view description, ImTextureID icon, float width, float height,
                      float globalScale, bool selected)
{
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList *draw = ImGui::GetWindowDrawList();

    bool pressed = ImGui::InvisibleButton(id, ImVec2(width, height));

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    if (hovered && !selected)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    ImU32 border = string_hex_to_rgba_u32("#BCBCBCFF");
    ImU32 iconColor = string_hex_to_rgba_u32("#D9D9D9FF");
    ImU32 nameColor = string_hex_to_rgba_u32("#FFFFFFFF");
    ImU32 descriptionColor = string_hex_to_rgba_u32("#CECECEFF");

    float opacity = selected ? 1.0f : active ? 0.85f : hovered ? 0.7f : 0.5f;

    border = u32_multiply_alpha(border, opacity);
    iconColor = u32_multiply_alpha(iconColor, opacity);
    nameColor = u32_multiply_alpha(nameColor, opacity);
    descriptionColor = u32_multiply_alpha(descriptionColor, opacity);

    float rounding = 8.0f * globalScale;
    float borderSize = 1.0f * globalScale;
    float nameFontSize = 13.0f * globalScale;
    float descriptionFontSize = 12.0f * globalScale;

    draw->AddRect(
        ImVec2(pos.x + (borderSize / 2.0f) - 1, pos.y + (borderSize / 2.0f) - 1),
        ImVec2(pos.x + width - (borderSize / 2.0f) + 1, pos.y + height - (borderSize / 2.0f) + 1),
        border, rounding, borderSize);

    const ImVec2 center(pos.x + (width * 0.5f), pos.y + (height * 0.5f));
    const float imageSize = 64.0f * globalScale;

    draw->AddImage(icon, ImVec2(center.x - (imageSize * 0.5f), pos.y + (10.0f * globalScale)),
                   ImVec2(center.x + (imageSize * 0.5f), pos.y + (10.0f * globalScale) + imageSize),
                   ImVec2(0, 0), ImVec2(1, 1), iconColor);

    ImVec2 nameSize =
        font->CalcTextSizeA(nameFontSize, FLT_MAX, 0.0f, name.data(), name.data() + name.size());
    draw->AddText(font, nameFontSize,
                  ImVec2(center.x - (nameSize.x / 2.0f),
                         pos.y + (10.0f * globalScale) + imageSize + (1.0f * globalScale)),
                  nameColor, name.data(), name.data() + name.size());

    const float textWidth = width - (35.0f * globalScale);

    AddAlignedWrappedText(draw, font, descriptionFontSize,
                          ImVec2(center.x - (textWidth * 0.5f),
                                 pos.y + (10.0f * globalScale) + imageSize + (1.0f * globalScale) +
                                     nameSize.y + (5.0f * globalScale)),
                          textWidth, descriptionColor, TextAlign::Center, description.data(),
                          description.data() + description.size());

    return pressed;
}

struct SwitchState
{
    bool initialized = false;
    bool last_value = false;
    float animation = 0.0f;
};

static std::unordered_map<ImGuiID, SwitchState> states;

static float EaseEmphasized(float t)
{
    t = std::clamp(t, 0.0f, 1.0f);

    // cubic ease-in-out
    if (t < 0.5f)
    {
        return 4.0f * t * t * t;
    }

    const float f = (2.0f * t) - 2.0f;
    return (0.5f * f * f * f) + 1.0f;
}

bool ui::ToggleSwitch(const char *label, bool *value, float animation_speed)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGuiWindow *window = ImGui::GetCurrentWindow();

    if (window->SkipItems)
    {
        return false;
    }

    ImGuiContext &g = *GImGui;

    const ImGuiID id = window->GetID(label);

    const float track_w = 52.0f * globalScale;
    const float track_h = 24.0f * globalScale;

    const float rounding = track_h * 0.5f;

    const float off_thumb_r = 8.0f * globalScale;
    const float on_thumb_r = 12.0f * globalScale;

    const ImVec2 pos = ImGui::GetCursorScreenPos();

    const ImVec2 track_min = pos;
    const ImVec2 track_max = ImVec2(pos.x + track_w, pos.y + track_h);
    const ImRect box(track_min, track_max);

    ImGui::ItemSize(ImVec2(track_w, track_h));

    if (!ImGui::ItemAdd(box, id))
    {
        return false;
    }

    SwitchState &state = states[id];

    if (!state.initialized)
    {
        state.initialized = true;
        state.last_value = *value;
        state.animation = *value ? 1.0f : 0.0f;
    }

    if (state.last_value != *value)
    {
        state.last_value = *value;
    }

    bool hovered = false;
    bool held = false;

    const bool pressed = ImGui::ButtonBehavior(box, id, &hovered, &held, ImGuiButtonFlags_None);

    if (pressed)
    {
        *value = !*value;
        state.last_value = *value;
    }

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    const float target = *value ? 1.0f : 0.0f;
    const float dt = g.IO.DeltaTime;

    if (std::abs(state.animation - target) > 0.0001f)
    {
        state.animation = ImLerp(state.animation, target, 1.0f - std::exp(-animation_speed * dt));
    }
    else
    {
        state.animation = target;
    }

    const float t = EaseEmphasized(state.animation);

    constexpr ImVec4 on_track = ImVec4(0.404f, 0.314f, 0.706f, 1.0f);
    constexpr ImVec4 on_thumb = ImVec4(1.000f, 1.000f, 1.000f, 1.0f);

    constexpr ImVec4 on_track_hover = ImVec4(0.455f, 0.365f, 0.765f, 1.0f);
    constexpr ImVec4 on_thumb_hover = ImVec4(0.965f, 0.950f, 1.000f, 1.0f);

    constexpr ImVec4 on_track_active = ImVec4(0.340f, 0.255f, 0.620f, 1.0f);
    constexpr ImVec4 on_thumb_active = ImVec4(0.930f, 0.915f, 0.990f, 1.0f);

    constexpr ImVec4 off_track = ImVec4(0.230f, 0.220f, 0.240f, 1.0f);
    constexpr ImVec4 off_thumb = ImVec4(0.580f, 0.560f, 0.600f, 1.0f);

    constexpr ImVec4 off_track_hover = ImVec4(0.275f, 0.265f, 0.285f, 1.0f);
    constexpr ImVec4 off_thumb_hover = ImVec4(0.650f, 0.630f, 0.670f, 1.0f);

    constexpr ImVec4 off_track_active = ImVec4(0.315f, 0.305f, 0.330f, 1.0f);
    constexpr ImVec4 off_thumb_active = ImVec4(0.710f, 0.690f, 0.730f, 1.0f);

    ImVec4 track_color;
    ImVec4 thumb_color;

    if (held)
    {
        track_color = ImLerp(off_track_active, on_track_active, t);
        thumb_color = ImLerp(off_thumb_active, on_thumb_active, t);
    }
    else if (hovered)
    {
        track_color = ImLerp(off_track_hover, on_track_hover, t);
        thumb_color = ImLerp(off_thumb_hover, on_thumb_hover, t);
    }
    else
    {
        track_color = ImLerp(off_track, on_track, t);
        thumb_color = ImLerp(off_thumb, on_thumb, t);
    }

    const ImVec4 outline_color = ImVec4(0.520f, 0.500f, 0.540f, 1.0f);

    ImDrawList *draw = window->DrawList;

    const float outline_alpha = 1.0f - t;

    if (outline_alpha > 0.001f)
    {
        float outlineSize = 1.0f * globalScale;
        draw->AddRectFilled(ImVec2(track_min.x - outlineSize, track_min.y - outlineSize),
                            ImVec2(track_max.x + outlineSize, track_max.y + outlineSize),
                            ImGui::ColorConvertFloat4ToU32(ImVec4(outline_color.x, outline_color.y,
                                                                  outline_color.z, outline_alpha)),
                            rounding + outlineSize);
    }

    draw->AddRectFilled(track_min, track_max, ImGui::ColorConvertFloat4ToU32(track_color),
                        rounding);

    const float thumb_r = ImLerp(off_thumb_r, on_thumb_r, t);

    const float left_x = track_min.x + off_thumb_r + 4.0f * globalScale;
    const float right_x = track_max.x - on_thumb_r - 4.0f * globalScale;

    const float thumb_x = ImLerp(left_x, right_x, t);

    const float thumb_y = track_min.y + track_h * 0.5f;

    // shadow
    draw->AddCircleFilled(ImVec2(thumb_x, thumb_y + 1.0f * globalScale),
                          thumb_r + 1.0f * globalScale, IM_COL32(0, 0, 0, 25));
    draw->AddCircleFilled(ImVec2(thumb_x, thumb_y), thumb_r,
                          ImGui::ColorConvertFloat4ToU32(thumb_color));

    if (hovered || held)
    {
        const float state_alpha = held ? 0.12f : 0.02f;

        const ImU32 state_color =
            ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, state_alpha));

        draw->AddCircleFilled(ImVec2(thumb_x, thumb_y), thumb_r + 4.0f * globalScale, state_color);
    }

    return pressed;
}

// NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-vararg)