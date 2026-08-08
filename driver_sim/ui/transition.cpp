#include <blackboard_app/gui.h>
#include <imgui/imgui.h>

#include <algorithm>

#include "transition.h"

using blackboard::gui::string_hex_to_rgba_u32;
using blackboard::gui::u32_multiply_alpha;
using namespace ui;

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
        state = TransitionState::None;
        alpha = 0.0f;
        return;
    }

    if (state != TransitionState::None)
    {
        return;
    }

    if (page == currentPage)
    {
        return;
    }

    targetPage = page;
    state = TransitionState::FadeToBackground;
    alpha = 0.0f;
}

void ui::Transition::update()
{
    if (state == TransitionState::None)
    {
        return;
    }

    const float deltaTime = ImGui::GetIO().DeltaTime;
    const float alphaStep = duration > 0.0f ? (deltaTime / duration) : 1.0f;

    if (state == TransitionState::FadeToBackground)
    {
        alpha = std::min(1.0f, alpha + alphaStep);
        if (alpha >= 1.0f)
        {
            currentPage = targetPage;
            state = TransitionState::FadeFromBackground;
        }
    }
    else if (state == TransitionState::FadeFromBackground)
    {
        alpha = std::max(0.0f, alpha - alphaStep);
        if (alpha <= 0.0f)
        {
            state = TransitionState::None;
        }
    }
}

void ui::Transition::draw()
{
    if (state == TransitionState::None && alpha <= 0.0f)
    {
        return;
    }

    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

    ImGui::GetWindowDrawList()->AddRectFilled(
        windowPos, windowMax,
        u32_multiply_alpha(string_hex_to_rgba_u32("#1E1E1Eff"), std::clamp(alpha, 0.0f, 1.0f)));
}