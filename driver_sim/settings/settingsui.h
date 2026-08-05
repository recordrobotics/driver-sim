#pragma once

#include <imgui/imgui.h>
#include <blackboard_app/gui.h>

namespace settings
{
    void init(blackboard::gui::ImTexture &logo);
    void cleanup();
    void draw(ImFont *font, ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize);
};