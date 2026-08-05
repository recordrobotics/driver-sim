#pragma once

#include <list>
#include <string>

namespace ui
{
    typedef struct ToggleButton
    {
        const char *label;
        bool *state;
    } ToggleButton;

    void DrawCenteredText(const char *text, float yOffset = 0.0f);
    void DrawProgress(const std::string_view &label, float value, bool isError = false);
    void SplitToggleButtonGroup(std::list<ToggleButton> buttons);
    bool UnderlineTextButton(const char *text);
    bool CircularButton(const char *id, float radius);

    bool IconButton(ImFont *font, const char *id, std::string_view text, ImTextureID icon, float size, float borderSize, float rounding, float fontSize, float textOffset, bool inverted = false);
}