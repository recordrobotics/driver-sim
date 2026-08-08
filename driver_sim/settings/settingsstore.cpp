#include "settingsstore.h"

#include <SDL3/SDL.h>
#include <bx/error.h>
#include <bx/file.h>
#include <nlohmann/json.hpp>

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace nlohmann
{
    template <> struct adl_serializer<settings::Rebuilt2026>
    {
        static void to_json(json &json, const settings::Rebuilt2026 &value)
        {
            json = {{"energizedRPThreshold", value.energizedRPThreshold},
                    {"superchargedRPThreshold", value.superchargedRPThreshold}};
        }

        static settings::Rebuilt2026 from_json(const json &json)
        {
            return {.energizedRPThreshold = json.at("energizedRPThreshold").get<int>(),
                    .superchargedRPThreshold = json.at("superchargedRPThreshold").get<int>()};
        }
    };

    template <> struct adl_serializer<CameraView>
    {
        static void to_json(json &json, const CameraView &value)
        {
            switch (value)
            {
            case CameraView::Field:
                json = "field";
                break;
            case CameraView::Robot:
                json = "robot";
                break;
            case CameraView::RobotRelative:
                json = "robot-relative";
                break;
            case CameraView::DriverStation:
                json = "driverstation";
                break;
            }
        }

        static CameraView from_json(const json &json)
        {
            std::string view = json.get<std::string>();
            if (view == "field")
            {
                return CameraView::Field;
            }
            if (view == "robot")
            {
                return CameraView::Robot;
            }
            if (view == "robot-relative")
            {
                return CameraView::RobotRelative;
            }
            if (view == "driverstation")
            {
                return CameraView::DriverStation;
            }

            throw std::invalid_argument("Invalid camera view");
        }
    };
} // namespace nlohmann

namespace settings
{
    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,readability-magic-numbers)

    bool enableTAA = true;
    bool enableMotionBlur = true;
    bool enableBloom = true;
    bool enableGTAO = true;
    bool writeObjectMotionVectors = true;
    bool enableVSync = true;
    bool enableDebugMenu = false;

    bool showMainMenu = true;
    bool cacheModels = true;
    std::vector<std::string> jvmArguments;
    std::vector<std::string> codeArguments;

    std::unordered_set<std::string> enabledExtensions;
    bool launchElastic = true;
    bool launchRobotCode = true;
    double ntPeriodic = 0.022;
    bool enableFrameInterpolation = true;

    bool enableDiscordSDK = true;

    uint64_t javaLogMaxBytes = 1024ull * 1024ull * 1024ull; // 1 GB

    uint32_t gameTeam = 6731;
    std::vector<uint32_t> gameTeamPool = {151, 69, 97, 4169, 246};
    uint32_t gameMatchType = 2;
    uint32_t gameMatchNumber = 42;
    uint32_t gameMatchTotal = 76;

    std::string renderApi = "auto";
    bool updateWhileMinimized = true;

    Rebuilt2026 rebuilt2026 = {.energizedRPThreshold = 100, .superchargedRPThreshold = 360};

    CameraView viewMode = CameraView::Field;
    float cameraFov = 60.0f;
    std::vector<uint32_t> cameraTarget = {0, 0};

    void loadDefaultSettings()
    {
        logger->info("Loading default settings.");
        enableTAA = true;
        enableMotionBlur = true;
        enableBloom = true;
        enableGTAO = true;
        writeObjectMotionVectors = true;
        enableVSync = true;
        enableDebugMenu = false;
        showMainMenu = true;
        cacheModels = true;
        jvmArguments = {};
        codeArguments = {};

        enableFrameInterpolation = true;
        updateWhileMinimized = true;
        renderApi = "auto";

        if (std::filesystem::exists(
                R"(C:\Program Files (x86)\FRC Driver Station\DriverStation.exe)"))
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

        enableDiscordSDK = true;

        javaLogMaxBytes = 1024ull * 1024ull * 1024ull; // 1 GB

        gameTeam = 6731;
        gameTeamPool = {151, 69, 97, 4169, 246};
        gameMatchType = 2;
        gameMatchNumber = 42;
        gameMatchTotal = 76;

        rebuilt2026 = {.energizedRPThreshold = 100, .superchargedRPThreshold = 360};

        viewMode = CameraView::Field;
        cameraFov = 60.0f;
        cameraTarget = {0, 0};
    }

    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,readability-magic-numbers)

    uint64_t parseHumanSizeToBytes(const std::string &sizeStr)
    {
        constexpr uint64_t MULTIPLIER_KB = 1024ull;
        constexpr uint64_t MULTIPLIER_MB = 1024ull * MULTIPLIER_KB;
        constexpr uint64_t MULTIPLIER_GB = 1024ull * MULTIPLIER_MB;
        constexpr uint64_t MULTIPLIER_TB = 1024ull * MULTIPLIER_GB;

        std::string numberPart;
        std::string unitPart;

        for (char chr : sizeStr)
        {
            if (std::isdigit(chr) != 0)
            {
                numberPart += chr;
            }
            else if (std::isalpha(chr) != 0)
            {
                unitPart += static_cast<char>(std::tolower(chr));
            }
        }

        uint64_t number = std::stoull(numberPart);
        uint64_t multiplier = 1;

        if (unitPart == "kb")
        {
            multiplier = MULTIPLIER_KB;
        }
        else if (unitPart == "mb")
        {
            multiplier = MULTIPLIER_MB;
        }
        else if (unitPart == "gb")
        {
            multiplier = MULTIPLIER_GB;
        }
        else if (unitPart == "tb")
        {
            multiplier = MULTIPLIER_TB;
        }

        return number * multiplier;
    }

    std::string humanReadableSize(uint64_t bytes)
    {
        constexpr uint64_t MULTIPLIER = 1024ull;
        constexpr std::array<const char *, 5> units = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;

        while (bytes >= MULTIPLIER && unitIndex < static_cast<int>(units.size()) - 1)
        {
            bytes /= MULTIPLIER;
            unitIndex++;
        }

        constexpr size_t MAX_BUFFER_SIZE = 64;

        std::array<char, MAX_BUFFER_SIZE> buffer{};
        std::snprintf(buffer.data(), buffer.size(), "%llu %s", bytes,
                      units.at(std::clamp(unitIndex, 0, static_cast<int>(units.size()) - 1)));
        return {buffer.data()};
    }

    void loadSettings()
    {
        std::string settingsPath =
            std::string(SDL_GetPrefPath(nullptr, "DriverSim")) + "settings.json";
        bx::Error error;
        bx::FileReader reader;

        if (bx::open(&reader, settingsPath.c_str(), &error))
        {
            const int64_t size = bx::getSize(&reader);
            if (size < 0)
            {
                logger->error("Failed to get size of settings file: {0}, error: {1}", settingsPath,
                              error.getMessage().getCPtr());
                loadDefaultSettings();
                return;
            }

            std::vector<char> data(size + 1);
            if (bx::read(&reader, data.data(), static_cast<int32_t>(size), &error) != size)
            {
                logger->error("Failed to read settings file: {0}, error: {1}", settingsPath,
                              error.getMessage().getCPtr());
                bx::close(&reader);
                loadDefaultSettings();
                return;
            }

            data[size] = '\0';
            nlohmann::json json = nlohmann::json::parse(data.data());

            enableTAA = json.value("enableTAA", enableTAA);
            enableMotionBlur = json.value("enableMotionBlur", enableMotionBlur);
            enableBloom = json.value("enableBloom", enableBloom);
            enableGTAO = json.value("enableGTAO", enableGTAO);
            writeObjectMotionVectors =
                json.value("writeObjectMotionVectors", writeObjectMotionVectors);
            enableVSync = json.value("enableVSync", enableVSync);
            enableDebugMenu = json.value("enableDebugMenu", enableDebugMenu);

            showMainMenu = json.value("showMainMenu", showMainMenu);
            cacheModels = json.value("cacheModels", cacheModels);
            jvmArguments = json.value("jvmArguments", jvmArguments);
            codeArguments = json.value("codeArguments", codeArguments);
            enabledExtensions = json.value("enabledExtensions", enabledExtensions);
            launchElastic = json.value("launchElastic", launchElastic);
            launchRobotCode = json.value("launchRobotCode", launchRobotCode);
            ntPeriodic = json.value("ntPeriodic", ntPeriodic);
            enableFrameInterpolation =
                json.value("enableFrameInterpolation", enableFrameInterpolation);

            enableDiscordSDK = json.value("enableDiscordSDK", enableDiscordSDK);

            javaLogMaxBytes = parseHumanSizeToBytes(
                json.value("javaLogMaxBytes", humanReadableSize(javaLogMaxBytes)));

            gameTeam = json.value("gameTeam", gameTeam);
            gameTeamPool = json.value("gameTeamPool", gameTeamPool);
            gameMatchType = json.value("gameMatchType", gameMatchType);
            gameMatchNumber = json.value("gameMatchNumber", gameMatchNumber);
            gameMatchTotal = json.value("gameMatchTotal", gameMatchTotal);

            renderApi = json.value("renderApi", renderApi);
            updateWhileMinimized = json.value("updateWhileMinimized", updateWhileMinimized);

            rebuilt2026 = json.value("rebuilt2026", rebuilt2026);

            viewMode = json.value("viewMode", viewMode);
            cameraFov = json.value("cameraFov", cameraFov);
            cameraTarget = json.value("cameraTarget", cameraTarget);

            logger->info("Settings loaded successfully from {0}", settingsPath);
        }
        else
        {
            logger->error("Could not open settings file: {0}, error: {1}", settingsPath,
                          error.getMessage().getCPtr());
            loadDefaultSettings();
        }
    }

    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    void saveSettings()
    {
        std::string settingsPath =
            std::string(SDL_GetPrefPath(nullptr, "DriverSim")) + "settings.json";

        nlohmann::json json;
        json["enableTAA"] = enableTAA;
        json["enableMotionBlur"] = enableMotionBlur;
        json["enableBloom"] = enableBloom;
        json["enableGTAO"] = enableGTAO;
        json["writeObjectMotionVectors"] = writeObjectMotionVectors;
        json["enableVSync"] = enableVSync;
        json["enableDebugMenu"] = enableDebugMenu;

        json["showMainMenu"] = showMainMenu;
        json["cacheModels"] = cacheModels;
        json["jvmArguments"] = jvmArguments;
        json["codeArguments"] = codeArguments;
        json["enabledExtensions"] = enabledExtensions;
        json["launchElastic"] = launchElastic;
        json["launchRobotCode"] = launchRobotCode;
        json["ntPeriodic"] = ntPeriodic;
        json["enableFrameInterpolation"] = enableFrameInterpolation;

        json["enableDiscordSDK"] = enableDiscordSDK;

        json["javaLogMaxBytes"] = humanReadableSize(javaLogMaxBytes);

        json["gameTeam"] = gameTeam;
        json["gameTeamPool"] = gameTeamPool;
        json["gameMatchType"] = gameMatchType;
        json["gameMatchNumber"] = gameMatchNumber;
        json["gameMatchTotal"] = gameMatchTotal;

        json["renderApi"] = renderApi;
        json["updateWhileMinimized"] = updateWhileMinimized;

        json["rebuilt2026"] = rebuilt2026;

        json["viewMode"] = viewMode;
        json["cameraFov"] = cameraFov;
        json["cameraTarget"] = cameraTarget;

        std::string jsonString = json.dump(4);
        bx::Error error;
        bx::FileWriter writer;

        if (bx::open(&writer, settingsPath.c_str(), false, &error))
        {
            if (bx::write(&writer, jsonString.data(), static_cast<int32_t>(jsonString.size()),
                          &error) != jsonString.size())
            {
                logger->error("Failed to write settings file: {0}, error: {1}", settingsPath,
                              error.getMessage().getCPtr());
            }
            else
            {
                logger->info("Settings saved successfully to {0}", settingsPath);
            }

            bx::close(&writer);
        }
        else
        {
            logger->error("Could not open settings file for writing: {0}, error: {1}", settingsPath,
                          error.getMessage().getCPtr());
        }
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
} // namespace settings