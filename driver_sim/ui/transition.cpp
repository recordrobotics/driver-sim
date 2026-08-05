#include <blackboard_app/gui.h>
#include <imgui/imgui.h>

#include <algorithm>

#include "transition.h"

using blackboard::gui::string_hex_to_rgba_float;

ui::Transition::Transition(int startingPage, float duration)
    : currentPage(startingPage), targetPage(startingPage), duration(duration)
{
}

void ui::Transition::transition(int page, bool instant)
{
    if (instant)
    {
        currentPage = page;
        targetPage = page;
        state = TRANSITION_NONE;
        alpha = 0.0f;
        return;
    }

    if (state != TRANSITION_NONE)
    {
        return;
    }

    if (page == currentPage)
    {
        return;
    }

    targetPage = page;
    state = TRANSITION_FADE_TO_BG;
    alpha = 0.0f;
}

void ui::Transition::update()
{
    if (state == TRANSITION_NONE)
    {
        return;
    }

    const float deltaTime = ImGui::GetIO().DeltaTime;
    const float alphaStep = duration > 0.0f ? (deltaTime / duration) : 1.0f;

    if (state == TRANSITION_FADE_TO_BG)
    {
        alpha = std::min(1.0f, alpha + alphaStep);
        if (alpha >= 1.0f)
        {
            currentPage = targetPage;
            state = TRANSITION_FADE_FROM_BG;
        }
    }
    else if (state == TRANSITION_FADE_FROM_BG)
    {
        alpha = std::max(0.0f, alpha - alphaStep);
        if (alpha <= 0.0f)
        {
            state = TRANSITION_NONE;
        }
    }
}

void ui::Transition::draw()
{
    if (state == TRANSITION_NONE && alpha <= 0.0f)
    {
        return;
    }

    ImVec4 overlayColor = string_hex_to_rgba_float("#1E1E1Eff");
    overlayColor.w = std::clamp(alpha, 0.0f, 1.0f);

    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

    ImGui::GetWindowDrawList()->AddRectFilled(windowPos, windowMax,
                                              ImGui::ColorConvertFloat4ToU32(overlayColor));
}