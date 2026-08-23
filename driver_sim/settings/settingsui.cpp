#include "settingsui.h"
#include "settingsstore.h"

#include "../ui/components.h"
#include "../utils.h"
#include <blackboard_app/gui.h>

#include <cstdint>
#include <format>
#include <ranges>
#include <string>
#include <type_traits>
#include <vector>

#include <logo.png.h>

#include <imgui/misc/cpp/imgui_stdlib.h>

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

    ui::DrawLinkText("https://github.com/recordrobotics/2026-robot");
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));
    ui::DrawLinkText("https://github.com/recordrobotics/driver-sim");
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
    ui::DrawVerticallyCenteredText(text, 60.0f * globalScale);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void drawSubHeader(const char *text)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 10.0f * globalScale);

    ImGui::Dummy(ImVec2(0, 20.0f * globalScale));
    ImGui::SameLine();
    ImGui::PushFont(nullptr, 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#9D9D9Dff"));
    ui::DrawVerticallyCenteredText(text, 20.0f * globalScale);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

template <typename T> struct SettingOptionData
{
    T *value;
    T defaultValue;
};

template <typename T>
void drawSettingOption(const char *id, const char *name, const char *description,
                       SettingOptionData<T> data)
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15.0f * globalScale);

    ImGui::PushFont(nullptr, 13.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8A8A8AFF"));
    ImGui::TextUnformatted(id);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 1.0f * globalScale));

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15.0f * globalScale);

    ImGui::PushFont(nullptr, 20.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#D1D1D1FF"));
    ImGui::TextUnformatted(name);
    ImGui::PopStyleColor();
    ImGui::PopFont();

    if constexpr (std::is_same_v<T, bool>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        ui::ToggleSwitch((std::string("##value_") + id).c_str(), data.value);
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputScalar((std::string("##value_") + id).c_str(), ImGuiDataType_S32,
                               data.value, nullptr, nullptr, "%d"))
        {
        }
    }
    else if constexpr (std::is_same_v<T, std::uint64_t>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputScalar((std::string("##value_") + id).c_str(), ImGuiDataType_U64,
                               data.value, nullptr, nullptr, "%llu"))
        {
        }
    }
    else if constexpr (std::is_same_v<T, std::uint32_t>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputScalar((std::string("##value_") + id).c_str(), ImGuiDataType_U32,
                               data.value, nullptr, nullptr, "%u"))
        {
        }
    }
    else if constexpr (std::is_same_v<T, float>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputScalar((std::string("##value_") + id).c_str(), ImGuiDataType_Float,
                               data.value, nullptr, nullptr, "%.4f"))
        {
        }
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputScalar((std::string("##value_") + id).c_str(), ImGuiDataType_Double,
                               data.value, nullptr, nullptr, "%.4f"))
        {
        }
    }
    else if constexpr (std::is_same_v<T, std::string>)
    {
        ImGui::SameLine(ImGui::GetWindowWidth() - style.WindowPadding.x - style.ScrollbarSize -
                        52.0f * globalScale);
        if (ImGui::InputText((std::string("##value_") + id).c_str(), data.value,
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
        }
    }

    ImGui::Dummy(ImVec2(0, 2.0f * globalScale));

    ImGui::Columns(2, nullptr, false);
    ImGui::SetColumnWidth(0, 355.0f * globalScale);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 15.0f * globalScale);
    ImGui::PushFont(nullptr, 12.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#A7A7A7FF"));
    ImGui::TextWrapped(description);
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10.0f * globalScale, 0));

    ImGui::NextColumn();

    if (*data.value != data.defaultValue)
    {
        ImGui::PushFont(nullptr, 11.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#F0F25FFF"));

        std::string str;
        if constexpr (std::is_same_v<T, bool>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::uint64_t>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::uint32_t>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::vector<std::uint32_t>>)
        {
            str = std::format(
                "This value is different from the default of '{}'.",
                string_join(data.defaultValue |
                                std::views::transform([](auto v) { return std::to_string(v); }),
                            ", "));
        }
        else if constexpr (std::is_same_v<T, float>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::string>)
        {
            str =
                std::format("This value is different from the default of '{}'.", data.defaultValue);
        }
        else if constexpr (std::is_same_v<T, std::vector<std::string>>)
        {
            str = std::format("This value is different from the default of '{}'.",
                              string_join(data.defaultValue, ", "));
        }

        ui::TextAlignedWrapped(ui::TextAlign::Right, str.c_str());
        ImGui::PopStyleColor();
        if (ui::DrawLinkText("Reset to default", ui::TextAlign::Right,
                             {.underline = true,
                              .color = string_hex_to_rgba_u32("#EC9658FF"),
                              .hoverColor = string_hex_to_rgba_u32("#de8647FF"),
                              .activeColor = string_hex_to_rgba_u32("#ba7849FF")},
                             (std::string("reset_") + id).c_str()))
        {
            *data.value = data.defaultValue;
        }
        ImGui::PopFont();
    }

    ImGui::Columns(1);

    if constexpr (std::is_same_v<T, std::vector<std::uint32_t>>)
    {
        ui::InputUInt32Vector((std::string("##value_") + id).c_str(), *data.value);
    }
    else if constexpr (std::is_same_v<T, std::vector<std::string>>)
    {
        ui::InputStringVector((std::string("##value_") + id).c_str(), *data.value);
    }
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
        ui::DrawVerticallyCenteredText("Settings", logoSize.y);
        ImGui::PopStyleColor();
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 20.0f * globalScale));

        auto spacer = ImVec2(0, 7.0f * globalScale);

        drawHeader("General");
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "showMainMenu", "Show main menu",
            "Determines whether the main menu should show when Driver Sim is started. When false "
            "Driver Sim opens directly to the 3D field view.",
            {.value = &settings::current.showMainMenu,
             .defaultValue = settings::makeDefault().showMainMenu});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>("showExitWarning", "Show exit warning",
                                "Determines whether to show the warning confirmation message when "
                                "leaving the 3D field view and going back to the main menu.",
                                {.value = &settings::current.showExitWarning,
                                 .defaultValue = settings::makeDefault().showExitWarning});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "launchRobotCode", "Launch robot code",
            "Whether to launch the robot code when opening the 3D field view. "
            "Disable if using a separate instance of the robot code, for example "
            "when working as a developer on the code.",
            {.value = &settings::current.launchRobotCode,
             .defaultValue = settings::makeDefault().launchRobotCode});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "launchElastic", "Launch Elastic",
            "Whether to launch the Elastic dashboard when opening the 3D field view. Disable if "
            "you prefer to not have the dashboard open or are using a separate instance of "
            "Elastic, for example when working as a developer on the robot code. Note that even if "
            "this is enabled, if an existing Elastic instance is already running it will NOT "
            "launch a second instance.",
            {.value = &settings::current.launchElastic,
             .defaultValue = settings::makeDefault().launchElastic});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableDiscordSDK", "Enable Discord SDK",
            "When enabled, the Discord SDK binary is downloaded and enables Discord "
            "integration features like Rich Presence. Requires restart.",
            {.value = &settings::current.enableDiscordSDK,
             .defaultValue = settings::makeDefault().enableDiscordSDK});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>("fullscreen", "Fullscreen window",
                                "When enabled, Driver Sim runs in fullscreen mode.",
                                {.value = &settings::current.fullscreen,
                                 .defaultValue = settings::makeDefault().fullscreen});
        ImGui::Dummy(spacer);

        drawHeader("Game Specific");
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>(
            "gameTeam", "Team number",
            "This is your team number. It is shown in the FMS ui at the position "
            "your alliance station is set to.",
            {.value = &settings::current.gameTeam,
             .defaultValue = settings::makeDefault().gameTeam});
        ImGui::Dummy(spacer);

        drawSettingOption<std::vector<uint32_t>>(
            "gameTeamPool", "Team pool",
            "These are the other 5 team numbers to populate the FMS ui with. The "
            "order is chosen randomly based on your alliance station.",
            {.value = &settings::current.gameTeamPool,
             .defaultValue = settings::makeDefault().gameTeamPool});
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>("gameMatchType", "Match type",
                                    "The match type shown in the FMS ui.",
                                    {.value = &settings::current.gameMatchType,
                                     .defaultValue = settings::makeDefault().gameMatchType});
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>("gameMatchNumber", "Match number",
                                    "The match number shown in the FMS ui.",
                                    {.value = &settings::current.gameMatchNumber,
                                     .defaultValue = settings::makeDefault().gameMatchNumber});
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>("gameMatchTotal", "Total matches",
                                    "The total match number shown in the FMS ui.",
                                    {.value = &settings::current.gameMatchTotal,
                                     .defaultValue = settings::makeDefault().gameMatchTotal});
        ImGui::Dummy(spacer);

        drawSubHeader("Rebuilt 2026");
        ImGui::Dummy(spacer);

        drawSettingOption<int>(
            "rebuilt2026.energizedRPThreshold", "Energized RP threshold",
            "The displayed energized RP threshold in the FMS score ui.",
            {.value = &settings::current.rebuilt2026.energizedRPThreshold,
             .defaultValue = settings::makeDefault().rebuilt2026.energizedRPThreshold});
        ImGui::Dummy(spacer);

        drawSettingOption<int>(
            "rebuilt2026.superchargedRPThreshold", "Supercharged RP threshold",
            "The displayed supercharged RP threshold in the FMS score ui.",
            {.value = &settings::current.rebuilt2026.superchargedRPThreshold,
             .defaultValue = settings::makeDefault().rebuilt2026.superchargedRPThreshold});
        ImGui::Dummy(spacer);

        drawSubHeader("3D Field");
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>(
            "viewMode", "View mode",
            "The targeting mode used by the camera in the 3D field. The preferred "
            "way to change this is in the View Mode menu.",
            {.value = reinterpret_cast<uint32_t *>(&settings::current.viewMode),
             .defaultValue = static_cast<uint32_t>(settings::makeDefault().viewMode)});
        ImGui::Dummy(spacer);

        drawSettingOption<float>(
            "cameraFov", "Camera field of view",
            "The vertical field of view in degrees to use for the camera in the 3D field.",
            {.value = &settings::current.cameraFov,
             .defaultValue = settings::makeDefault().cameraFov});
        ImGui::Dummy(spacer);

        drawSettingOption<std::vector<uint32_t>>(
            "cameraTarget", "Camera robot target",
            "The robot to target when the camera is in one of the robot view modes. "
            "The preferred way to change this is in the View Mode menu.",
            {.value = &settings::current.cameraTarget,
             .defaultValue = settings::makeDefault().cameraTarget});
        ImGui::Dummy(spacer);

        drawHeader("Simulation");
        ImGui::Dummy(spacer);

        drawSettingOption<std::unordered_set<std::string>>(
            "enabledExtensions", "Enabled extensions",
            "The extensions to enable when running the simulation. The preferred way "
            "to change these is in the main menu.",
            {.value = &settings::current.enabledExtensions,
             .defaultValue = settings::makeDefault().enabledExtensions});
        ImGui::Dummy(spacer);

        drawSettingOption<std::vector<std::string>>(
            "jvmArguments", "JVM arguments",
            "The arguments to pass to the JVM when running the simulation. Use this "
            "to set system flags or properties.",
            {.value = &settings::current.jvmArguments,
             .defaultValue = settings::makeDefault().jvmArguments});
        ImGui::Dummy(spacer);

        drawSettingOption<std::vector<std::string>>(
            "codeArguments", "Code arguments",
            "The arguments to pass to the robot code main method when running the simulation.",
            {.value = &settings::current.codeArguments,
             .defaultValue = settings::makeDefault().codeArguments});
        ImGui::Dummy(spacer);

        drawSettingOption<uint64_t>(
            "javaLogMaxBytes", "Max log size",
            "The maximum size of the robot code logs. Accepts values in the general formats: 64, "
            "64 b(B), 8 kb(KB), 64 mb(MB), 2 gb(GB), 1 tb(TB), etc. When this limit is reached "
            "Driver Sim deletes oldest logs first until enough storage space is restored. This "
            "process is run on both startup and exit.",
            {.value = &settings::current.javaLogMaxBytes,
             .defaultValue = settings::makeDefault().javaLogMaxBytes});
        ImGui::Dummy(spacer);

        drawSettingOption<double>(
            "ntPeriodic", "NetworkTables update interval",
            "The interval in seconds at which to subscribe for updates to the simulation "
            "NetworkTables server. A larger interval results in a more “sluggish” or “overly "
            "smooth” behavior, while a smaller interval results in a more responsive experience "
            "but can occasionally stutter depending on system performance. It is recommended to "
            "set this value slightly above the robot simulation periodic interval to avoid "
            "aliasing issues with the frame interpolation logic.",
            {.value = &settings::current.ntPeriodic,
             .defaultValue = settings::makeDefault().ntPeriodic});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableFrameInterpolation", "Enable Frame Interpolation",
            "Whether to smoothly interpolate between NetworkTables updates for clearer motion on "
            "the screen and to support motion blur. When disabled, objects only move when a new "
            "position is received from the simulation, resulting in jerky movements if the render "
            "frame rate is faster than the update interval.",
            {.value = &settings::current.enableFrameInterpolation,
             .defaultValue = settings::makeDefault().enableFrameInterpolation});
        ImGui::Dummy(spacer);

        drawHeader("Graphics");
        ImGui::Dummy(spacer);

        drawSettingOption<std::string>(
            "renderApi", "Render API",
            "Specify which rendering API Driver Sim should use. Auto uses 'vulkan' on Windows and "
            "'metal' on macOS. It is strongly recommended to use either 'vulkan' or 'd3d12' on "
            "Windows due to graphical inconsistencies on d3d11. Requires restart.",
            {.value = &settings::current.renderApi,
             .defaultValue = settings::makeDefault().renderApi});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableVSync", "Enable V-Sync",
            "When enabled, the frame rate is capped at your screen's refresh rate. "
            "If disabled you may see screen tearing artifacts when your frame rate "
            "is higher than the refresh rate.",
            {.value = &settings::current.enableVSync,
             .defaultValue = settings::makeDefault().enableVSync});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "updateWhileMinimized", "Update while minimized",
            "When disabled, Driver Sim pauses all periodic work until the window is "
            "shown again. A brief visual artifact will be seen as Driver Sim catches "
            "up with the robot simulation after being restored. Note that the robot "
            "simulation keeps running in the background regardless.",
            {.value = &settings::current.updateWhileMinimized,
             .defaultValue = settings::makeDefault().updateWhileMinimized});
        ImGui::Dummy(spacer);

        drawSettingOption<uint32_t>(
            "useFullDetailRobotModel", "Use full detail robot model",
            "The full detail robot model is the original unmodified 3D model provided by the robot "
            "asset. Driver Sim automatically performs simplification and optimization steps on the "
            "model to increase performance. This setting lets you optionally choose where to bring "
            "back the full detail robot model.",
            {.value = reinterpret_cast<uint32_t *>(&settings::current.useFullDetailRobotModel),
             .defaultValue =
                 static_cast<uint32_t>(settings::makeDefault().useFullDetailRobotModel)});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "cacheModels", "Cache 3D models",
            "Whether to cache the pre-processed 3D model files. When enabled, this significantly "
            "lowers startup time (by a factor of 10x or more) however uses around 2-3x more disk "
            "space. It is strongly recommended to keep this enabled if possible.",
            {.value = &settings::current.cacheModels,
             .defaultValue = settings::makeDefault().cacheModels});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "writeObjectMotionVectors", "Write object motion vectors",
            "Whether to compute and write motion vectors for moving objects such as the robot or "
            "game pieces on the field. This does not affect motion vectors generated from camera "
            "movement. When enabled, motion blur is added when the robot or game pieces move. If "
            "the robot code simulation does not support identities for Pose3d's it is recommended "
            "to disable this to avoid possible visual glitches from objects changing their order "
            "in the array.",
            {.value = &settings::current.writeObjectMotionVectors,
             .defaultValue = settings::makeDefault().writeObjectMotionVectors});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableGTAO", "Enable GTAO",
            "This effect adds realistic ambient occlusion on opaque geometry such as "
            "in corners where light can't easily reach the surface. Massively "
            "improves visual quality but decreases performance slightly.",
            {.value = &settings::current.enableGTAO,
             .defaultValue = settings::makeDefault().enableGTAO});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableTAA", "Enable TAA",
            "This effect adds temporal antialiasing which smooths pixelated lines and aliased "
            "details on screen. Improves general visual quality but can introduce ghosting "
            "artifacts when geometry gets deoccluded as objects or the camera move around. It is "
            "recommended to keep this enabled when GTAO is enabled to provide additional temporal "
            "denoising effects. It is strongly recommended to keep writeObjectMotionVectors "
            "enabled when TAA is enabled to avoid distracting blurring and smudging artifacts on "
            "the robot and game pieces in motion.",
            {.value = &settings::current.enableTAA,
             .defaultValue = settings::makeDefault().enableTAA});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableMotionBlur", "Enable motion blur",
            "This effect adds a motion blur on the screen when the camera or objects "
            "move quickly. Improves visual quality on fast moving game pieces (like "
            "balls being shot) but can decrease performance slightly.",
            {.value = &settings::current.enableMotionBlur,
             .defaultValue = settings::makeDefault().enableMotionBlur});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>(
            "enableBloom", "Enable bloom",
            "When enabled, this adds a glow effect to bright elements on the screen. "
            "Massively improves visual quality for lights and LEDs on the robot and "
            "field but can decrease performance slightly.",
            {.value = &settings::current.enableBloom,
             .defaultValue = settings::makeDefault().enableBloom});
        ImGui::Dummy(spacer);

        drawSettingOption<bool>("enableDebugMenu", "Enable debug menu",
                                "When enabled, shows a debug menu on the 3D field view. Useful for "
                                "changing effect settings or viewing debug data from shaders.",
                                {.value = &settings::current.enableDebugMenu,
                                 .defaultValue = settings::makeDefault().enableDebugMenu});
        ImGui::Dummy(spacer);

        drawAboutPanel(font);
    }

    ImGui::End();

    ImGui::PopStyleVar(2);
}