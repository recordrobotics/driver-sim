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
    extern bool writeObjectMotionVectors;

    extern bool showSelectPage;
    extern bool cacheModels;
    extern std::string extraArguments;

    extern std::unordered_set<std::string> enabledExtensions;
    extern bool launchElastic;
    extern bool launchRobotCode;
    extern double ntPeriodic;

    extern uint64_t javaLogMaxBytes;
}