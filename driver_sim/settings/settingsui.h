#pragma once

#include <blackboard_app/gui.h>
#include <imgui/imgui.h>

namespace settings
{
    void init(blackboard::gui::ImTexture &logo);
    void cleanup();
    void draw(ImFont *font, ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize);
}; // namespace settings