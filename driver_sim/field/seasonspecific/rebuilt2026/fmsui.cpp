#include <blackboard_app/logger.h>

#include "fmsui.h"

#include "../../../settings/settingsstore.h"

#include <rebuilt2026/firstage.png.h>
#include <rebuilt2026/rebuilt.png.h>
#include <rebuilt2026/arrow.png.h>
#include <rebuilt2026/fuelblue.png.h>
#include <rebuilt2026/fuelred.png.h>

#include "hublights.h"

using namespace blackboard::logger;

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
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

Rebuilt2026FMSUI::Rebuilt2026FMSUI(nt::NetworkTableInstance &ntInst)
{
    matchTimeTopic = ntInst.GetDoubleTopic("/AdvantageKit/DriverStation/MatchTime");
    matchTimeSub = matchTimeTopic.Subscribe(-1.0, {.periodic = settings::ntPeriodic});

    redHubActiveTopic = ntInst.GetBooleanTopic("/SmartDashboard/MapleSim/MatchData/Breakdown/Red Alliance/Improved Active");
    redHubActiveSub = redHubActiveTopic.Subscribe(false, {.periodic = settings::ntPeriodic});

    blueHubActiveTopic = ntInst.GetBooleanTopic("/SmartDashboard/MapleSim/MatchData/Breakdown/Blue Alliance/Improved Active");
    blueHubActiveSub = blueHubActiveTopic.Subscribe(false, {.periodic = settings::ntPeriodic});

    redScoreTopic = ntInst.GetDoubleTopic("/SmartDashboard/MapleSim/MatchData/Breakdown/Red Alliance/Improved Score");
    redScoreSub = redScoreTopic.Subscribe(0.0, {.periodic = settings::ntPeriodic});

    blueScoreTopic = ntInst.GetDoubleTopic("/SmartDashboard/MapleSim/MatchData/Breakdown/Blue Alliance/Improved Score");
    blueScoreSub = blueScoreTopic.Subscribe(0.0, {.periodic = settings::ntPeriodic});

    isAutonomousTopic = ntInst.GetBooleanTopic("/AdvantageKit/DriverStation/Autonomous");
    isAutonomousSub = isAutonomousTopic.Subscribe(false, {.periodic = settings::ntPeriodic});

    allianceStationTopic = ntInst.GetIntegerTopic("/AdvantageKit/DriverStation/AllianceStation");
    allianceStationSub = allianceStationTopic.Subscribe(1, {.periodic = settings::ntPeriodic});

    font = blackboard::gui::get_font("Roboto_Bold_ttf");

    load_image((void *)firstage_png_bytes, sizeof(firstage_png_bytes), firstAgeBanner);
    load_image((void *)rebuilt_png_bytes, sizeof(rebuilt_png_bytes), rebuiltBanner);
    load_image((void *)arrow_png_bytes, sizeof(arrow_png_bytes), arrowIcon);
    load_image((void *)fuelblue_png_bytes, sizeof(fuelblue_png_bytes), fuelBlueIcon);
    load_image((void *)fuelred_png_bytes, sizeof(fuelred_png_bytes), fuelRedIcon);
}

Rebuilt2026FMSUI::~Rebuilt2026FMSUI()
{
    firstAgeBanner.destroy();
    rebuiltBanner.destroy();
    arrowIcon.destroy();
    fuelBlueIcon.destroy();
    fuelRedIcon.destroy();
}

void Rebuilt2026FMSUI::render(ImVec2 winSize)
{
    drawFMSUI(winSize);
    updateHubMaterials();
}

void Rebuilt2026FMSUI::updateHubMaterials()
{
    if (hubRedLightMaterial)
    {
        hubRedLightMaterial->emissionColor = redHubActiveSub.Get() ? Rebuilt2026::redHubLedColor : Rebuilt2026::hubLedOffColor;
    }
    if (hubBlueLightMaterial)
    {
        hubBlueLightMaterial->emissionColor = blueHubActiveSub.Get() ? Rebuilt2026::blueHubLedColor : Rebuilt2026::hubLedOffColor;
    }
}

void Rebuilt2026FMSUI::drawFMSUI(ImVec2 winSize)
{
    teamAssigner.update(allianceStationSub.Get());
    logoCache.update();

    ImGuiViewport *viewport = ImGui::GetMainViewport();

    float scale = std::min(winSize.x / canvasWidth, winSize.y / canvasHeight);
    float offsetX = (winSize.x - canvasWidth * scale) / 2.0f + viewport->Pos.x;
    float offsetY = viewport->Pos.y;

    auto Transform = [scale, offsetX, offsetY](ImVec2 p)
    {
        return ImVec2(
            p.x * scale + offsetX,
            p.y * scale + offsetY);
    };

    ImDrawList *drawList = ImGui::GetBackgroundDrawList(viewport);

    auto Rect = [&Transform, scale, drawList](ImVec2 topLeft, ImVec2 size, ImU32 color)
    {
        topLeft = Transform(topLeft);
        size.x *= scale;
        size.y *= scale;
        drawList->AddRectFilled(ImVec2(topLeft.x, topLeft.y), ImVec2(topLeft.x + size.x, topLeft.y + size.y), color);
    };

    auto Text = [this, &Transform, scale, drawList](ImVec2 pos, const char *text, float fontSize, ImU32 color, bool centered = false)
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
    if (settings::gameMatchType == 0)
        centerText = "Test Match";
    else if (settings::gameMatchType == 1)
        centerText = "Practice ";
    else if (settings::gameMatchType == 2)
        centerText = "Qualification ";
    else if (settings::gameMatchType == 3)
        centerText = "Elimination ";
    else
        centerText = "Unknown Match Type ";

    Text({960, 37.5f}, (settings::gameMatchType == 0 ? centerText : (centerText + std::to_string(settings::gameMatchNumber) + " of " + std::to_string(settings::gameMatchTotal))).c_str(), 32, IM_COL32_WHITE, true);

    // Banners
    Rect({450, 15}, {180, 45}, LIGHT);
    if (firstAgeBanner.id)
    {
        drawList->AddImage(firstAgeBanner.id, Transform({450, 15}), Transform({450 + 180, 15 + 45}));
    }
    Rect({1290, 15}, {180, 45}, LIGHT);
    if (rebuiltBanner.id)
    {
        drawList->AddImage(rebuiltBanner.id, Transform({1290, 15}), Transform({1290 + 180, 15 + 45}));
    }

    // blue panel
    Rect({450, 60}, {450, 70}, BLUE);
    // red panel
    Rect({1020, 60}, {450, 70}, RED);

    int blueScore = static_cast<int>(blueScoreSub.Get());
    int redScore = static_cast<int>(redScoreSub.Get());

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
    constexpr int blueScoreCX = blueScoreX0 + blueScoreW / 2;
    constexpr int blueScoreCY = teamY + teamPanelH; // bottom edge

    constexpr int redScoreX0 = 1020;     // start of red panel
    constexpr int redScoreX1 = redTeamX; // start of team numbers
    constexpr int redScoreW = redScoreX1 - redScoreX0;
    constexpr int redScoreH = teamPanelH;
    constexpr int redScoreCX = redScoreX0 + redScoreW / 2;
    constexpr int redScoreCY = teamY + teamPanelH; // bottom edge

    // Blue score
    Text({blueScoreCX, blueScoreCY - blueScoreH / 2}, std::to_string(blueScore).c_str(), 48, IM_COL32_WHITE, true);

    // Red score
    Text({redScoreCX, redScoreCY - redScoreH / 2}, std::to_string(redScore).c_str(), 48, IM_COL32_WHITE, true);

    // Team numbers
    std::array<uint32_t, 6> teamNumbers = teamAssigner.getTeamNumbers();
    for (int i = 0; i < teamNumbers.size(); ++i)
    {
        float cellX = (i < 3 ? redTeamX : blueTeamX) + (i % 3) * teamCellW;

        // Background
        ImU32 cellColor;
        if (i < 3)
            cellColor = (i % 3) == 1 ? RED_TEAM_CENTER : RED_TEAM_OUTER;
        else
            cellColor = (i % 3) == 1 ? BLUE_TEAM_CENTER : BLUE_TEAM_OUTER;

        Rect({cellX, teamY}, {teamCellW, teamPanelH}, cellColor);

        // Logo
        float logoX = cellX + 12;
        float logoY = teamY + 15;
        constexpr int logoW = 30;
        constexpr int logoH = 30;
        int teamNumber = teamNumbers[i];
        ImTexture logoTex = logoCache.getTeamLogo(teamNumber);
        if (logoTex.id)
        {
            drawList->AddImage(logoTex.id, Transform({logoX, logoY}), Transform({logoX + logoW, logoY + logoH}));
        }

        // Team Number
        Text({cellX + (teamCellW + 30) / 2, teamY + 30}, std::to_string(teamNumber).c_str(), 21, IM_COL32_WHITE, true);
    }

    // Match time
    int matchTime = static_cast<int>(std::ceil(matchTimeSub.Get()));
    if (matchTime < 0)
        matchTime = 140;
    int minutes = matchTime / 60;
    int seconds = matchTime % 60;

    constexpr int timerPanelX = 450;
    constexpr int timerPanelY = 70;
    constexpr int timerPanelW = 1020;
    constexpr int timerPanelH = 60;
    constexpr int timerCY = timerPanelY + timerPanelH;

    Text({960, timerCY - timerPanelH / 2}, (std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds)).c_str(), 45, IM_COL32_BLACK, true);

    // Shift timer
    if (!isAutonomousSub.Get())
    {
        constexpr int shiftPanelX = 450 + 450; // right edge of blue panel
        constexpr int shiftPanelY = timerPanelY + timerPanelH + 1;
        constexpr int shiftPanelW = 1020 - shiftPanelX;
        constexpr int shiftPanelH = 38;
        constexpr ImU32 borderColor = string_hex_to_rgba_u32("#dddbdcff");

        // Horizontal separator
        Rect({shiftPanelX, shiftPanelY - 1}, {shiftPanelW, 2}, borderColor);
        // Outer border
        Rect({shiftPanelX - 1, shiftPanelY - 1}, {shiftPanelW + 2, shiftPanelH + 2}, borderColor);

        int shiftNum;
        int shiftTimeLeft;

        if (matchTime > 130)
        {
            shiftNum = 1;
            shiftTimeLeft = matchTime - 130;
        }
        else if (matchTime > 105)
        {
            shiftNum = 2;
            shiftTimeLeft = matchTime - 105;
        }
        else if (matchTime > 80)
        {
            shiftNum = 3;
            shiftTimeLeft = matchTime - 80;
        }
        else if (matchTime > 55)
        {
            shiftNum = 4;
            shiftTimeLeft = matchTime - 55;
        }
        else if (matchTime > 30)
        {
            shiftNum = 5;
            shiftTimeLeft = matchTime - 30;
        }
        else
        {
            shiftNum = 6;
            shiftTimeLeft = matchTime;
        }

        // Shift count panel
        constexpr int shiftCountX = shiftPanelX;
        constexpr int shiftCountY = shiftPanelY;
        constexpr int shiftCountW = 75;
        Rect({shiftCountX, shiftCountY}, {shiftCountW, shiftPanelH}, IM_COL32_WHITE);
        Text({shiftCountX + shiftCountW / 2, shiftCountY + shiftPanelH / 2}, (std::to_string(shiftNum) + " / 6").c_str(), 32, IM_COL32_BLACK, true);

        // Shift timer panel
        constexpr int shiftTimerX = shiftCountX + shiftCountW;
        constexpr int shiftTimerW = shiftPanelW - shiftCountW;
        Rect({shiftTimerX, shiftCountY}, {shiftTimerW, shiftPanelH}, borderColor);
        Text({shiftTimerX + shiftTimerW / 2, shiftCountY + shiftPanelH / 2}, ((shiftTimeLeft < 10 ? ":0" : ":") + std::to_string(shiftTimeLeft)).c_str(), 21, IM_COL32_BLACK, true);
    }

    // Blue fuel counter
    Rect({35, 67}, {45, 45}, IM_COL32_WHITE);
    if (fuelBlueIcon.id)
    {
        drawList->AddImage(fuelBlueIcon.id, Transform({35, 67}), Transform({35 + 45, 67 + 45}));
    }
    Rect({80, 67}, {150, 45}, BLUE);
    int rankingPointThresholdBlue = blueScore >= settings::rebuilt2026.energizedRPThreshold ? settings::rebuilt2026.superchargedRPThreshold : settings::rebuilt2026.energizedRPThreshold;
    Text({155, 89.5f}, (std::to_string(blueScore) + " / " + std::to_string(rankingPointThresholdBlue)).c_str(), 32, IM_COL32_WHITE, true);

    // Red fuel counter
    Rect({1689, 67}, {45, 45}, IM_COL32_WHITE);
    if (fuelRedIcon.id)
    {
        drawList->AddImage(fuelRedIcon.id, Transform({1689, 67}), Transform({1689 + 45, 67 + 45}));
    }
    Rect({1734, 67}, {150, 45}, RED);
    int rankingPointThresholdRed = redScore >= settings::rebuilt2026.energizedRPThreshold ? settings::rebuilt2026.superchargedRPThreshold : settings::rebuilt2026.energizedRPThreshold;
    Text({1809, 89.5f}, (std::to_string(redScore) + " / " + std::to_string(rankingPointThresholdRed)).c_str(), 32, IM_COL32_WHITE, true);

    // Left arrow
    if (blueHubActiveSub.Get())
    {
        if (arrowIcon.id)
        {
            drawList->AddImage(arrowIcon.id, Transform({302, 67}), Transform({302 + 45, 67 + 45}), {1, 0}, {0, 1}); // flip horizontally
        }
        else
        {
            Rect({302, 67}, {45, 45}, YELLOW);
        }
    }

    // Right arrow
    if (redHubActiveSub.Get())
    {
        if (arrowIcon.id)
        {
            drawList->AddImage(arrowIcon.id, Transform({1573, 67}), Transform({1573 + 45, 67 + 45}));
        }
        else
        {
            Rect({1573, 67}, {45, 45}, YELLOW);
        }
    }
}

void Rebuilt2026FMSUI::postProcessField(std::vector<Mesh> &fieldMeshes)
{
    hubRedLightMaterial = Rebuilt2026::getTaggedMaterial(fieldMeshes, Rebuilt2026::redHubLedTag);
    hubBlueLightMaterial = Rebuilt2026::getTaggedMaterial(fieldMeshes, Rebuilt2026::blueHubLedTag);
    logger->info("Post-processed field meshes for hub light materials: red={}, blue={}", (hubRedLightMaterial != nullptr), (hubBlueLightMaterial != nullptr));
}