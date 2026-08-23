#pragma once

#include <list>
#include <string>
#include <vector>

namespace ui
{
    struct ToggleButton
    {
        const char *label;
        bool *state;
    };

    enum class TextAlign : uint8_t
    {
        Left,
        Center,
        Right
    };

    struct LinkTextOptions
    {
        bool underline = false;
        ImU32 color = blackboard::gui::string_hex_to_rgba_u32("#6C74FAFF");
        ImU32 hoverColor = blackboard::gui::string_hex_to_rgba_u32("#5b63f0FF");
        ImU32 activeColor = blackboard::gui::string_hex_to_rgba_u32("#767ce3FF");
    };

    void DrawCenteredText(const char *text, float yOffset = 0.0f);
    void DrawProgress(const std::string_view &label, float value, bool isError = false);
    void SplitToggleButtonGroup(const std::list<ToggleButton> &buttons);
    bool UnderlineTextButton(const char *text);
    bool CircularButton(const char *id, float radius);

    void TextAlignedWrapped(TextAlign align, const char *text);
    void DrawVerticallyCenteredText(const char *text, float heightAvailable);
    bool DrawLinkText(const char *label, ui::TextAlign align = ui::TextAlign::Left,
                      ui::LinkTextOptions options = {}, const char *id = nullptr,
                      const char *url = nullptr);

    bool IconButton(ImFont *font, const char *id, std::string_view text, ImTextureID icon,
                    float size, float borderSize, float rounding, float fontSize, float textOffset,
                    float opacity, bool inverted = false);

    bool ChoiceButton(ImFont *font, const char *id, std::string_view name,
                      std::string_view description, ImTextureID icon, float width, float height,
                      float globalScale, bool selected = false);

    bool ToggleSwitch(const char *label, bool *value, float animation_speed = 12.0f);
    bool InputUInt32Vector(const char *label, std::vector<std::uint32_t> &values,
                           ImVec2 size = ImVec2(0, 0));
    bool InputStringVector(const char *label, std::vector<std::string> &values,
                           ImVec2 size = ImVec2(0, 0));
} // namespace ui