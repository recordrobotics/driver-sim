#include "settingsstore.h"

#include <bx/file.h>
#include <bx/error.h>
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace nlohmann
{
    template <>
    struct adl_serializer<settings::Rebuilt2026>
    {
        static void to_json(json &j, const settings::Rebuilt2026 &value)
        {
            j = {
                {"energizedRPThreshold", value.energizedRPThreshold},
                {"superchargedRPThreshold", value.superchargedRPThreshold}};
        }

        static settings::Rebuilt2026 from_json(const json &j)
        {
            return {
                j.at("energizedRPThreshold").get<int>(),
                j.at("superchargedRPThreshold").get<int>()};
        }
    };
}

namespace settings
{
    bool enableTAA = true;
    bool enableMotionBlur = true;
    bool enableBloom = true;
    bool writeObjectMotionVectors = true;

    bool showSelectPage = true;
    bool cacheModels = true;
    std::vector<std::string> jvmArguments;
    std::vector<std::string> codeArguments;

    std::unordered_set<std::string> enabledExtensions;
    bool launchElastic = true;
    bool launchRobotCode = true;
    double ntPeriodic = 0.022;

    uint64_t javaLogMaxBytes = 1024ull * 1024ull * 1024ull; // 1 GB

    uint32_t gameTeam = 6731;
    std::vector<uint32_t> gameTeamPool = {
        151,
        69,
        97,
        4169,
        246};
    uint32_t gameMatchType = 2;
    uint32_t gameMatchNumber = 42;
    uint32_t gameMatchTotal = 76;

    Rebuilt2026 rebuilt2026 = {
        .energizedRPThreshold = 100,
        .superchargedRPThreshold = 360};

    void loadDefaultSettings()
    {
        logger->info("Loading default settings.");
        enableTAA = true;
        enableMotionBlur = true;
        enableBloom = true;
        writeObjectMotionVectors = true;

        showSelectPage = true;
        cacheModels = true;
        jvmArguments = {};
        codeArguments = {};

        if (std::filesystem::exists("C:\\Program Files (x86)\\FRC Driver Station\\DriverStation.exe"))
        {
            enabledExtensions = {"halsim_ds_socket"};
        }
        else
        {
            enabledExtensions = {"halsim_gui"};
        }

        launchElastic = true;
        launchRobotCode = true;
        ntPeriodic = 0.022;
        javaLogMaxBytes = 1024ull * 1024ull * 1024ull; // 1 GB

        gameTeam = 6731;
        gameTeamPool = {
            151,
            69,
            97,
            4169,
            246};
        gameMatchType = 2;
        gameMatchNumber = 42;
        gameMatchTotal = 76;

        rebuilt2026 = {
            .energizedRPThreshold = 100,
            .superchargedRPThreshold = 360};
    }

    uint64_t parseHumanSizeToBytes(const std::string &sizeStr)
    {
        std::string numberPart;
        std::string unitPart;

        for (char c : sizeStr)
        {
            if (std::isdigit(c))
            {
                numberPart += c;
            }
            else if (std::isalpha(c))
            {
                unitPart += std::tolower(c);
            }
        }

        uint64_t number = std::stoull(numberPart);
        uint64_t multiplier = 1;

        if (unitPart == "kb")
            multiplier = 1024ull;
        else if (unitPart == "mb")
            multiplier = 1024ull * 1024ull;
        else if (unitPart == "gb")
            multiplier = 1024ull * 1024ull * 1024ull;
        else if (unitPart == "tb")
            multiplier = 1024ull * 1024ull * 1024ull * 1024ull;

        return number * multiplier;
    }

    std::string humanReadableSize(uint64_t bytes)
    {
        const char *units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;

        while (bytes >= 1024ull && unitIndex < 4)
        {
            bytes /= 1024ull;
            unitIndex++;
        }

        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%llu %s", bytes, units[unitIndex]);
        return std::string(buffer);
    }

    void loadSettings()
    {
        std::string settingsPath = std::string(SDL_GetPrefPath(NULL, "DriverSim")) + "settings.json";
        bx::Error error;
        bx::FileReader reader;

        if (bx::open(&reader, settingsPath.c_str(), &error))
        {
            const int64_t size = bx::getSize(&reader);
            if (size < 0)
            {
                logger->error("Failed to get size of settings file: {0}, error: {1}", settingsPath, error.getMessage().getCPtr());
                loadDefaultSettings();
                return;
            }

            std::vector<char> data(size + 1);
            if (bx::read(&reader, data.data(), size, &error) != size)
            {
                logger->error("Failed to read settings file: {0}, error: {1}", settingsPath, error.getMessage().getCPtr());
                bx::close(&reader);
                loadDefaultSettings();
                return;
            }

            data[size] = '\0';
            nlohmann::json j = nlohmann::json::parse(data.data());

            enableTAA = j.value("enableTAA", enableTAA);
            enableMotionBlur = j.value("enableMotionBlur", enableMotionBlur);
            enableBloom = j.value("enableBloom", enableBloom);
            writeObjectMotionVectors = j.value("writeObjectMotionVectors", writeObjectMotionVectors);

            showSelectPage = j.value("showSelectPage", showSelectPage);
            cacheModels = j.value("cacheModels", cacheModels);
            jvmArguments = j.value("jvmArguments", jvmArguments);
            codeArguments = j.value("codeArguments", codeArguments);
            enabledExtensions = j.value("enabledExtensions", enabledExtensions);
            launchElastic = j.value("launchElastic", launchElastic);
            launchRobotCode = j.value("launchRobotCode", launchRobotCode);
            ntPeriodic = j.value("ntPeriodic", ntPeriodic);
            javaLogMaxBytes = parseHumanSizeToBytes(j.value("javaLogMaxBytes", humanReadableSize(javaLogMaxBytes)));

            gameTeam = j.value("gameTeam", gameTeam);
            gameTeamPool = j.value("gameTeamPool", gameTeamPool);
            gameMatchType = j.value("gameMatchType", gameMatchType);
            gameMatchNumber = j.value("gameMatchNumber", gameMatchNumber);
            gameMatchTotal = j.value("gameMatchTotal", gameMatchTotal);

            rebuilt2026 = j.value("rebuilt2026", rebuilt2026);

            logger->info("Settings loaded successfully from {0}", settingsPath);
        }
        else
        {
            logger->error("Could not open settings file: {0}, error: {1}", settingsPath, error.getMessage().getCPtr());
            loadDefaultSettings();
        }
    }

    void saveSettings()
    {
        std::string settingsPath = std::string(SDL_GetPrefPath(NULL, "DriverSim")) + "settings.json";

        nlohmann::json j;
        j["enableTAA"] = enableTAA;
        j["enableMotionBlur"] = enableMotionBlur;
        j["enableBloom"] = enableBloom;
        j["writeObjectMotionVectors"] = writeObjectMotionVectors;

        j["showSelectPage"] = showSelectPage;
        j["cacheModels"] = cacheModels;
        j["jvmArguments"] = jvmArguments;
        j["codeArguments"] = codeArguments;
        j["enabledExtensions"] = enabledExtensions;
        j["launchElastic"] = launchElastic;
        j["launchRobotCode"] = launchRobotCode;
        j["ntPeriodic"] = ntPeriodic;
        j["javaLogMaxBytes"] = humanReadableSize(javaLogMaxBytes);

        j["gameTeam"] = gameTeam;
        j["gameTeamPool"] = gameTeamPool;
        j["gameMatchType"] = gameMatchType;
        j["gameMatchNumber"] = gameMatchNumber;
        j["gameMatchTotal"] = gameMatchTotal;

        j["rebuilt2026"] = rebuilt2026;

        std::string jsonString = j.dump(4);
        bx::Error error;
        bx::FileWriter writer;

        if (bx::open(&writer, settingsPath.c_str(), false, &error))
        {
            if (bx::write(&writer, jsonString.data(), jsonString.size(), &error) != jsonString.size())
            {
                logger->error("Failed to write settings file: {0}, error: {1}", settingsPath, error.getMessage().getCPtr());
            }
            else
            {
                logger->info("Settings saved successfully to {0}", settingsPath);
            }

            bx::close(&writer);
        }
        else
        {
            logger->error("Could not open settings file for writing: {0}, error: {1}", settingsPath, error.getMessage().getCPtr());
        }
    }
}