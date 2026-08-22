#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

enum class CameraView : uint8_t
{
    Field,
    Robot,
    RobotRelative,
    DriverStation
};

enum class RobotModelSimplificationMode : uint8_t
{
    Never,
    Distance,
    MainRobot,
    Always
};

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