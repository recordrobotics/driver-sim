#include <blackboard_app/logger.h>

#include "fmsui.h"

#include "../../../settings/settingsstore.h"

#include "../../mesh.h"

using namespace blackboard::logger;

using blackboard::gui::ImTexture;
using blackboard::gui::string_hex_to_rgba_u32;

constexpr float canvasWidth = 1920.0f;
constexpr float canvasHeight = 1080.0f;

constexpr ImU32 BLUE = string_hex_to_rgba_u32("#1e5ac8ff");
constexpr ImU32 RED = string_hex_to_rgba_u32("#c82828ff");
constexpr ImU32 DARK = string_hex_to_rgba_u32("#141419ff");
constexpr ImU32 LIGHT = string_hex_to_rgba_u32("#e6e6e6ff");
constexpr ImU32 YELLOW = string_hex_to_rgba_u32("#fdf503ff");
constexpr ImU32 BLUE_TEAM_OUTER = string_hex_to_rgba_u32("#003f73ff");
constexpr ImU32 BLUE_TEAM_CENTER = string_hex_to_rgba_u32("#002e54ff");
constexpr ImU32 RED_TEAM_OUTER = string_hex_to_rgba_u32("#820c14ff");
constexpr ImU32 RED_TEAM_CENTER = string_hex_to_rgba_u32("#61090cff");

GenericFMSUI::GenericFMSUI(std::shared_ptr<FMS> fms) : fms(std::move(fms))
{
    font = blackboard::gui::get_font("Roboto_Bold_ttf");
}

void GenericFMSUI::render(ImVec2 winSize) { drawFMSUI(winSize); }

void GenericFMSUI::drawFMSUI(ImVec2 winSize)
{
    teamAssigner.update(fms->getAllianceStation());
    logoCache.update();

    ImGuiViewport *viewport = ImGui::GetMainViewport();

    float scale = std::min(winSize.x / canvasWidth, winSize.y / canvasHeight);
    float offsetX = ((winSize.x - (canvasWidth * scale)) / 2.0f) + viewport->Pos.x;
    float offsetY = viewport->Pos.y;

    auto Transform = [scale, offsetX, offsetY](ImVec2 point)
    { return ImVec2((point.x * scale) + offsetX, (point.y * scale) + offsetY); };

    ImDrawList *drawList = ImGui::GetBackgroundDrawList(viewport);

    auto Rect = [&Transform, scale, drawList](ImVec2 topLeft, ImVec2 size, ImU32 color)
    {
        topLeft = Transform(topLeft);
        size.x *= scale;
        size.y *= scale;
        drawList->AddRectFilled(ImVec2(topLeft.x, topLeft.y),
                                ImVec2(topLeft.x + size.x, topLeft.y + size.y), color);
    };

    auto Text = [this, &Transform, scale, drawList](ImVec2 pos, const char *text, float fontSize,
                                                    ImU32 color, bool centered = false)
    {
        pos = Transform(pos);

        if (centered)
        {
            ImVec2 textSize = font->CalcTextSizeA(fontSize * scale, FLT_MAX, 0.0f, text);
            pos.x -= textSize.x / 2.0f;
            pos.y -= textSize.y / 2.0f;
        }

        drawList->AddText(font, fontSize * scale, pos, color, text);
    };

    // center time panel
    Rect({450, 15}, {1020, 45}, DARK);
    Rect({450, 60}, {1020, 70}, IM_COL32_WHITE);

    // Center bar text
    std::string centerText;
    if (settings::current.gameMatchType == 0)
    {
        centerText = "Test Match";
    }
    else if (settings::current.gameMatchType == 1)
    {
        centerText = "Practice ";
    }
    else if (settings::current.gameMatchType == 2)
    {
        centerText = "Qualification ";
    }
    else if (settings::current.gameMatchType == 3)
    {
        centerText = "Elimination ";
    }
    else
    {
        centerText = "Unknown Match Type ";
    }

    Text({960, 37.5f},
         (settings::current.gameMatchType == 0
              ? centerText
              : (centerText + std::to_string(settings::current.gameMatchNumber) + " of " +
                 std::to_string(settings::current.gameMatchTotal)))
             .c_str(),
         32, IM_COL32_WHITE, true);

    // blue panel
    Rect({450, 60}, {450, 70}, BLUE);
    // red panel
    Rect({1020, 60}, {450, 70}, RED);

    constexpr int blueTeamX = 450;
    constexpr int redTeamX = 1140;
    constexpr int teamY = 70;
    constexpr int teamPanelW = 330;
    constexpr int teamPanelH = 60;
    constexpr int teamCellW = teamPanelW / 3;

    constexpr int blueScoreX0 = blueTeamX + teamPanelW;
    constexpr int blueScoreX1 = 450 + 450; // end of blue panel
    constexpr int blueScoreW = blueScoreX1 - blueScoreX0;
    constexpr int blueScoreH = teamPanelH;
    constexpr int blueScoreCX = blueScoreX0 + (blueScoreW / 2);
    constexpr int blueScoreCY = teamY + teamPanelH; // bottom edge

    constexpr int redScoreX0 = 1020;     // start of red panel
    constexpr int redScoreX1 = redTeamX; // start of team numbers
    constexpr int redScoreW = redScoreX1 - redScoreX0;
    constexpr int redScoreH = teamPanelH;
    constexpr int redScoreCX = redScoreX0 + (redScoreW / 2);
    constexpr int redScoreCY = teamY + teamPanelH; // bottom edge

    // Blue score
    Text({blueScoreCX, blueScoreCY - (blueScoreH / 2)}, std::to_string(fms->getBlueScore()).c_str(),
         48, IM_COL32_WHITE, true);

    // Red score
    Text({redScoreCX, redScoreCY - (redScoreH / 2)}, std::to_string(fms->getRedScore()).c_str(), 48,
         IM_COL32_WHITE, true);

    // Team numbers
    std::array<uint32_t, 6> teamNumbers = teamAssigner.getTeamNumbers();
    for (int i = 0; i < teamNumbers.size(); ++i)
    {
        float cellX = static_cast<float>((i < 3 ? redTeamX : blueTeamX) + ((i % 3) * teamCellW));

        // Background
        ImU32 cellColor{};
        if (i < 3)
        {
            cellColor = (i % 3) == 1 ? RED_TEAM_CENTER : RED_TEAM_OUTER;
        }
        else
        {
            cellColor = (i % 3) == 1 ? BLUE_TEAM_CENTER : BLUE_TEAM_OUTER;
        }

        Rect({cellX, teamY}, {teamCellW, teamPanelH}, cellColor);

        // Logo
        float logoX = cellX + 12;
        float logoY = teamY + 15;
        constexpr int logoW = 30;
        constexpr int logoH = 30;
        int teamNumber = teamNumbers[i];
        ImTexture logoTex = logoCache.getTeamLogo(teamNumber);
        if (logoTex.id != NULL)
        {
            drawList->AddImage(logoTex.id, Transform({logoX, logoY}),
                               Transform({logoX + logoW, logoY + logoH}));
        }

        // Team Number
        Text({cellX + ((teamCellW + 30) / 2), teamY + 30}, std::to_string(teamNumber).c_str(), 21,
             IM_COL32_WHITE, true);
    }

    // Match time
    int matchTime = fms->getMatchTime();
    if (matchTime < 0)
    {
        matchTime = 140;
    }
    int minutes = matchTime / 60;
    int seconds = matchTime % 60;

    constexpr int timerPanelX = 450;
    constexpr int timerPanelY = 70;
    constexpr int timerPanelW = 1020;
    constexpr int timerPanelH = 60;
    constexpr int timerCY = timerPanelY + timerPanelH;

    Text({960, timerCY - (timerPanelH / 2)},
         (std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds))
             .c_str(),
         45, IM_COL32_BLACK, true);
}