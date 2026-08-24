#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

template <typename T> struct SettingOptionData
{
    T *value;
    T defaultValue;
};

template <typename T> constexpr std::string_view enum_to_string(T value);
template <typename T> constexpr auto enum_all_values();

template <typename T>
concept ReflectableEnum = std::is_enum_v<T> && requires(T value) {
    { enum_to_string(value) } -> std::same_as<std::string_view>;
    { enum_all_values<T>() } -> std::ranges::forward_range;
    requires std::same_as<std::ranges::range_value_t<decltype(enum_all_values<T>())>, T>;
};

enum class CameraView : uint8_t
{
    Field,
    Robot,
    RobotRelative,
    DriverStation
};

template <> constexpr std::string_view enum_to_string<CameraView>(CameraView value)
{
    switch (value)
    {
    case CameraView::Field:
        return "Field";
    case CameraView::Robot:
        return "Robot";
    case CameraView::RobotRelative:
        return "Robot Relative";
    case CameraView::DriverStation:
        return "Driver Station";
    default:
        return "Unknown";
    }
}

template <> constexpr auto enum_all_values<CameraView>()
{
    return std::array<CameraView, 4>{CameraView::Field, CameraView::Robot,
                                     CameraView::RobotRelative, CameraView::DriverStation};
}

enum class RobotModelSimplificationMode : uint8_t
{
    Never,
    Distance,
    MainRobot,
    Always
};

template <>
constexpr std::string_view
enum_to_string<RobotModelSimplificationMode>(RobotModelSimplificationMode value)
{
    switch (value)
    {
    case RobotModelSimplificationMode::Never:
        return "Never";
    case RobotModelSimplificationMode::Distance:
        return "By Distance";
    case RobotModelSimplificationMode::MainRobot:
        return "Only Main Robot";
    case RobotModelSimplificationMode::Always:
        return "Always";
    default:
        return "Unknown";
    }
}

template <> constexpr auto enum_all_values<RobotModelSimplificationMode>()
{
    return std::array<RobotModelSimplificationMode, 4>{
        RobotModelSimplificationMode::Never, RobotModelSimplificationMode::Distance,
        RobotModelSimplificationMode::MainRobot, RobotModelSimplificationMode::Always};
}

namespace settings
{
    struct Store
    {
        // General

        bool showMainMenu;
        bool showExitWarning;
        bool launchRobotCode;
        bool launchElastic;
        bool enableDiscordSDK;
        bool fullscreen;

        // Game Specific

        uint32_t gameTeam;
        std::vector<uint32_t> gameTeamPool;
        uint32_t gameMatchType;
        uint32_t gameMatchNumber;
        uint32_t gameMatchTotal;

        struct Rebuilt2026
        {
            int energizedRPThreshold;
            int superchargedRPThreshold;
        };

        Rebuilt2026 rebuilt2026;

        CameraView viewMode;
        float cameraFov;
        std::vector<uint32_t> cameraTarget;

        // Simulation

        std::unordered_set<std::string> enabledExtensions;
        std::vector<std::string> jvmArguments;
        std::vector<std::string> codeArguments;
        uint64_t javaLogMaxBytes;
        double ntPeriodic;
        bool enableFrameInterpolation;

        // Graphics

        std::string renderApi;
        bool enableVSync;
        bool updateWhileMinimized;
        RobotModelSimplificationMode useFullDetailRobotModel;
        bool cacheModels;
        bool writeObjectMotionVectors;
        bool enableGTAO;
        bool enableTAA;
        bool enableMotionBlur;
        bool enableBloom;
        bool enableDebugMenu;
    };

    void loadDefaultSettings();
    void loadSettings();
    void saveSettings();

    Store makeDefault();

    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
    extern Store current;
    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)
} // namespace settings