#include "settingsui.h"

#include "../ui/components.h"
#include <blackboard_app/gui.h>

#include <logo.png.h>

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;
using blackboard::gui::string_hex_to_rgba_u32;

namespace settings
{
    ImTexture logo = {};
};

void settings::init(ImTexture &logo) { settings::logo = logo; }

void settings::cleanup() {}

void DrawVerticallyCenteredText(const char *text, float heightAvailable)
{
    ImVec2 textSize = ImGui::CalcTextSize(text);

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ((heightAvailable - textSize.y) * 0.5f));
    ImGui::TextUnformatted(text);
}

bool DrawLinkText(const char *label, const char *url = nullptr)
{
    if (url == nullptr)
    {
        url = label;
    }

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::CalcTextSize(label);
    ImDrawList *draw = ImGui::GetWindowDrawList();

    bool pressed = ImGui::InvisibleButton(label, size);

    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImU32 col = string_hex_to_rgba_u32("#6C74FAFF");

    if (active)
    {
        col = string_hex_to_rgba_u32("#767ce3FF");
    }
    else if (hovered)
    {
        col = string_hex_to_rgba_u32("#5b63f0FF");
    }

    if (hovered)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    if (pressed)
    {
        ImGuiPlatformIO &platform_io = ImGui::GetPlatformIO();
        if (platform_io.Platform_OpenInShellFn != nullptr)
        {
            platform_io.Platform_OpenInShellFn(ImGui::GetCurrentContext(), url);
        }
    }

    draw->AddText(pos, col, label);

    return pressed;
}

void drawAboutPanel(ImFont *font)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::Dummy(ImVec2(0, 50.0f * globalScale));

    ImGui::PushFont(nullptr, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#D3D3D3FF"));
    ImGui::TextUnformatted("About Driver Sim");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 5.0f * globalScale));

    ImGui::PushFont(nullptr, 10.0f);

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A2A2A2FF"));
    ImGui::TextUnformatted("Version 1.0.0 (2026) Build 2026.7.16+ (Packaged) dc88a1a d1d8cba");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::PopStyleColor();

    DrawLinkText("https://github.com/recordrobotics/2026-robot");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    DrawLinkText("https://github.com/recordrobotics/driver-sim");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A2A2A2FF"));

    ImGui::TextUnformatted("Stored Assets");
    ImGui::Dummy(ImVec2(0, 6.0f * globalScale));

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "jdk:8c7cfff78a55c56ebaf470ed6a89c6466b47d8274bdabdda997d7507c20325c5 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "elastic:6581e66eb237f9d615afb94077d89a03e2cdd7ce2d57f11c8cc5153821493ad7 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "field:0f2abde864422367dd1bc3254da23b36a3d82eb727d5dac0a0f2231bdc397e31 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "robot:b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "jni:0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5 (remote)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "code:7998021ca2a0f0d8867173cd7fcf8f4b15fb36d011d98df55b00bebb76732878 (packaged)");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 7.0f * globalScale);
    ImGui::TextUnformatted(
        "discord_sdk:2a7c8b043ca04a14a10c64b4f1116fe2a93bb6f6f4f0b4784c0ca1fc06ca832e (remote)");

    ImGui::Dummy(ImVec2(0, 6.0f * globalScale));

    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#858585FF"));
    ImGui::TextUnformatted("Made by Record Robotics");
    ImGui::PopStyleColor();

    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 10.0f * globalScale));
}

void drawHeader(const char *text)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::Dummy(ImVec2(0, 60.0f * globalScale));
    ImGui::SameLine();
    ImGui::PushFont(nullptr, 24.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#928E8Eff"));
    DrawVerticallyCenteredText(text, 60.0f * globalScale);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void drawSubHeader(const char *text)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::Dummy(ImVec2(0, 20.0f * globalScale));
    ImGui::SameLine();
    ImGui::PushFont(nullptr, 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#9D9D9Dff"));
    DrawVerticallyCenteredText(text, 20.0f * globalScale);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void drawSettingOption(const char *id, const char *name, const char *description)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::PushFont(nullptr, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8A8A8AFF"));
    ImGui::TextUnformatted(id);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));

    ImGui::PushFont(nullptr, 20.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#D1D1D1FF"));
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 2.0f * globalScale));

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 370.0f * globalScale);
    ImGui::PushFont(nullptr, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A7A7A7FF"));
    ImGui::TextWrapped(description);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10.0f * globalScale, 0));

    ImGui::NextColumn();

    ImGui::PushFont(nullptr, 11.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#F0F25FFF"));
    ui::TextAlignedWrapped(ui::TextAlign::Right,
                           "This value is different from the default of 'true'.");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#EC9658FF"));
    ui::TextAlignedWrapped(ui::TextAlign::Right, "Reset to default");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Columns(1);
}

void settings::draw(ImFont *font, ImGuiID viewportId, ImVec2 viewportPos, ImVec2 viewportSize)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    const ImVec2 padding = ImVec2(50.0f * globalScale, 40.0f * globalScale);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);

    ImGui::SetNextWindowPos(viewportPos);
    ImGui::SetNextWindowSize(viewportSize);
    ImGui::SetNextWindowViewport(viewportId);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking;
    if (ImGui::Begin("Settings", nullptr, flags))
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() - 10.0f * globalScale);
        const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);

        // Logo
        if (logo.id != 0u)
        {
            ImGui::Image(logo.id, logoSize);
        }
        else
        {
            ImGui::Dummy(logoSize);
        }

        ImGui::SameLine();
        ImGui::Dummy(ImVec2(28.0f * globalScale, 0));

        ImGui::SameLine();
        ImGui::PushFont(nullptr, 35.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8F8686ff"));
        DrawVerticallyCenteredText("Settings", logoSize.y);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 20.0f * globalScale));

        drawHeader("General");

        drawSettingOption(
            "showMainMenu", "Show main menu",
            "Determines whether the main menu should show when Driver Sim is started. When false "
            "Driver Sim opens directly to the 3D field view.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("showExitWarning", "Show exit warning",
                          "Determines whether to show the warning confirmation message when "
                          "leaving the 3D field view and going back to the main menu.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("launchRobotCode", "Launch robot code",
                          "Whether to launch the robot code when opening the 3D field view. "
                          "Disable if using a separate instance of the robot code, for example "
                          "when working as a developer on the code.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "launchElastic", "Launch Elastic",
            "Whether to launch the Elastic dashboard when opening the 3D field view. Disable if "
            "you prefer to not have the dashboard open or are using a separate instance of "
            "Elastic, for example when working as a developer on the robot code. Note that even if "
            "this is enabled, if an existing Elastic instance is already running it will NOT "
            "launch a second instance.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableDiscordSDK", "Enable Discord SDK",
                          "When enabled, the Discord SDK binary is downloaded and enables Discord "
                          "integration features like Rich Presence. Requires restart.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("fullscreen", "Fullscreen window",
                          "When enabled, Driver Sim runs in fullscreen mode.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawHeader("Game Specific");

        drawSettingOption("gameTeam", "Team number",
                          "This is your team number. It is shown in the FMS ui at the position "
                          "your alliance station is set to.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("gameTeamPool", "Team pool",
                          "These are the other 5 team numbers to populate the FMS ui with. The "
                          "order is chosen randomly based on your alliance station.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("gameMatchType", "Match type", "The match type shown in the FMS ui.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("gameMatchNumber", "Match number",
                          "The match number shown in the FMS ui.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("gameMatchTotal", "Total matches",
                          "The total match number shown in the FMS ui.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSubHeader("Rebuilt 2026");

        drawSettingOption("rebuilt2026.energizedRPThreshold", "Energized RP threshold",
                          "The displayed energized RP threshold in the FMS score ui.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("rebuilt2026.superchargedRPThreshold", "Supercharged RP threshold",
                          "The displayed supercharged RP threshold in the FMS score ui.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSubHeader("3D Field");

        drawSettingOption("viewMode", "View mode",
                          "The targeting mode used by the camera in the 3D field. The preferred "
                          "way to change this is in the View Mode menu.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "cameraFov", "Camera field of view",
            "The vertical field of view in degrees to use for the camera in the 3D field.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("cameraTarget", "Camera robot target",
                          "The robot to target when the camera is in one of the robot view modes. "
                          "The preferred way to change this is in the View Mode menu.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawHeader("Simulation");

        drawSettingOption("enabledExtensions", "Enabled extensions",
                          "The extensions to enable when running the simulation. The preferred way "
                          "to change these is in the main menu.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("jvmArguments", "JVM arguments",
                          "The arguments to pass to the JVM when running the simulation. Use this "
                          "to set system flags or properties.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "codeArguments", "Code arguments",
            "The arguments to pass to the robot code main method when running the simulation.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "javaLogMaxBytes", "Max log size",
            "The maximum size of the robot code logs. Accepts values in the general formats: 64, "
            "64 b(B), 8 kb(KB), 64 mb(MB), 2 gb(GB), 1 tb(TB), etc. When this limit is reached "
            "Driver Sim deletes oldest logs first until enough storage space is restored. This "
            "process is run on both startup and exit.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "ntPeriodic", "NetworkTables update interval",
            "The interval in seconds at which to subscribe for updates to the simulation "
            "NetworkTables server. A larger interval results in a more “sluggish” or “overly "
            "smooth” behavior, while a smaller interval results in a more responsive experience "
            "but can occasionally stutter depending on system performance. It is recommended to "
            "set this value slightly above the robot simulation periodic interval to avoid "
            "aliasing issues with the frame interpolation logic.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "enableFrameInterpolation", "Enable Frame Interpolation",
            "Whether to smoothly interpolate between NetworkTables updates for clearer motion on "
            "the screen and to support motion blur. When disabled, objects only move when a new "
            "position is received from the simulation, resulting in jerky movements if the render "
            "frame rate is faster than the update interval.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawHeader("Graphics");

        drawSettingOption(
            "renderApi", "Render API",
            "Specify which rendering API Driver Sim should use. Auto uses 'd3d11' on Windows and "
            "'metal' on macOS. It is strongly recommended to use either 'vulkan' or 'd3d12' on "
            "Windows due to graphical inconsistencies on d3d11. Requires restart.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableVSync", "Enable V-Sync",
                          "When enabled, the frame rate is capped at your screen’s refresh rate. "
                          "If disabled you may see screen tearing artifacts when your frame rate "
                          "is higher than the refresh rate.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("updateWhileMinimized", "Update while minimized",
                          "When disabled, Driver Sim pauses all periodic work until the window is "
                          "shown again. A brief visual artifact will be seen as Driver Sim catches "
                          "up with the robot simulation after being restored. Note that the robot "
                          "simulation keeps running in the background regardless.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "useFullDetailRobotModel", "Use full detail robot model",
            "The full detail robot model is the original unmodified 3D model provided by the robot "
            "asset. Driver Sim automatically performs simplification and optimization steps on the "
            "model to increase performance. This setting lets you optionally choose where to bring "
            "back the full detail robot model.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "cacheModels", "Cache 3D models",
            "Whether to cache the pre-processed 3D model files. When enabled, this significantly "
            "lowers startup time (by a factor of 10x or more) however uses around 2-3x more disk "
            "space. It is strongly recommended to keep this enabled if possible.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "writeObjectMotionVectors", "Write object motion vectors",
            "Whether to compute and write motion vectors for moving objects such as the robot or "
            "game pieces on the field. This does not affect motion vectors generated from camera "
            "movement. When enabled, motion blur is added when the robot or game pieces move. If "
            "the robot code simulation does not support identities for Pose3d's it is recommended "
            "to disable this to avoid possible visual glitches from objects changing their order "
            "in the array.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableGTAO", "Enable GTAO",
                          "This effect adds realistic ambient occlusion on opaque geometry such as "
                          "in corners where light can't easily reach the surface. Massively "
                          "improves visual quality but decreases performance slightly.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption(
            "enableTAA", "Enable TAA",
            "This effect adds temporal antialiasing which smooths pixelated lines and aliased "
            "details on screen. Improves general visual quality but can introduce ghosting "
            "artifacts when geometry gets deoccluded as objects or the camera move around. It is "
            "recommended to keep this enabled when GTAO is enabled to provide additional temporal "
            "denoising effects. It is strongly recommended to keep writeObjectMotionVectors "
            "enabled when TAA is enabled to avoid distracting blurring and smudging artifacts on "
            "the robot and game pieces in motion.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableMotionBlur", "Enable motion blur",
                          "This effect adds a motion blur on the screen when the camera or objects "
                          "move quickly. Improves visual quality on fast moving game pieces (like "
                          "balls being shot) but can decrease performance slightly.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableBloom", "Enable bloom",
                          "When enabled, this adds a glow effect to bright elements on the screen. "
                          "Massively improves visual quality for lights and LEDs on the robot and "
                          "field but can decrease performance slightly.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawSettingOption("enableDebugMenu", "Enable debug menu",
                          "When enabled, shows a debug menu on the 3D field view. Useful for "
                          "changing effect settings or viewing debug data from shaders.");
        ImGui::Dummy(ImVec2(0, 7.0f * globalScale));

        drawAboutPanel(font);
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
}