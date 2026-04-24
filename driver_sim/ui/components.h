#pragma once

#include <list>

namespace ui
{
    typedef struct ToggleButton
    {
        const char *label;
        bool *state;
    } ToggleButton;

    void DrawCenteredText(const char *text, float yOffset = 0.0f);
    void DrawProgress(const char *label, float value);
    void SplitToggleButtonGroup(std::list<ToggleButton> buttons);
    bool UnderlineTextButton(const char *text);
    bool CircularButton(const char *id, float radius);
}