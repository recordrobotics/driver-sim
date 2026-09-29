#pragma once

#include <blackboard_app/gui.h>
#include <imgui/imgui.h>

namespace settings
{
    void init(blackboard::gui::ImTexture &logo, blackboard::gui::ImTexture &mainMenuIcon);
    void cleanup();
    void draw(ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize, ImVec2 scrollbarAreaSize,
              bool noBringToFrontOnFocus = true);

    bool &showSettings();
}; // namespace settings