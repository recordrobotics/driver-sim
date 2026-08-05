#pragma once

#include <string>
#include <unordered_set>

namespace settings
{
    void loadDefaultSettings();
    void loadSettings();
    void saveSettings();

    extern bool enableTAA;
    extern bool enableMotionBlur;
    extern bool enableBloom;
    extern bool enableGTAO;
    extern bool writeObjectMotionVectors;
    extern bool enableVSync;
    extern bool enableDebugMenu;

    extern bool showMainMenu;
    extern bool cacheModels;
    extern std::vector<std::string> jvmArguments;
    extern std::vector<std::string> codeArguments;

    extern std::unordered_set<std::string> enabledExtensions;
    extern bool launchElastic;
    extern bool launchRobotCode;
    extern double ntPeriodic;
    extern bool enableFrameInterpolation;

    extern bool enableDiscordSDK;

    extern uint64_t javaLogMaxBytes;

    extern uint32_t gameTeam;
    extern std::vector<uint32_t> gameTeamPool;
    extern uint32_t gameMatchType;
    extern uint32_t gameMatchNumber;
    extern uint32_t gameMatchTotal;

    extern std::string renderApi;
    extern bool updateWhileMinimized;

    typedef struct Rebuilt2026
    {
        int energizedRPThreshold;
        int superchargedRPThreshold;
    } Rebuilt2026;

    extern Rebuilt2026 rebuilt2026;
}