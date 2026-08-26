#include "settingsstore.h"

#include <SDL3/SDL.h>
#include <bx/error.h>
#include <bx/file.h>
#include <nlohmann/json.hpp>

#include <blackboard_app/logger.h>

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

using namespace blackboard::logger;

namespace nlohmann
{
    template <> struct adl_serializer<settings::Store::Rebuilt2026>
    {
        static void to_json(json &json, const settings::Store::Rebuilt2026 &value)
        {
            json = {{"energizedRPThreshold", value.energizedRPThreshold},
                    {"superchargedRPThreshold", value.superchargedRPThreshold}};
        }

        static settings::Store::Rebuilt2026 from_json(const json &json)
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

    template <> struct adl_serializer<RobotModelSimplificationMode>
    {
        static void to_json(json &json, const RobotModelSimplificationMode &value)
        {
            switch (value)
            {
            case RobotModelSimplificationMode::Never:
                json = "never";
                break;
            case RobotModelSimplificationMode::Distance:
                json = "distance";
                break;
            case RobotModelSimplificationMode::MainRobot:
                json = "main-robot";
                break;
            case RobotModelSimplificationMode::Always:
                json = "always";
                break;
            }
        }

        static RobotModelSimplificationMode from_json(const json &json)
        {
            std::string mode = json.get<std::string>();
            if (mode == "never")
            {
                return RobotModelSimplificationMode::Never;
            }
            if (mode == "distance")
            {
                return RobotModelSimplificationMode::Distance;
            }
            if (mode == "main-robot")
            {
                return RobotModelSimplificationMode::MainRobot;
            }
            if (mode == "always")
            {
                return RobotModelSimplificationMode::Always;
            }

            throw std::invalid_argument("Invalid robot model simplification mode");
        }
    };
} // namespace nlohmann

namespace settings
{
    Store current;

    // NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables,readability-magic-numbers)
    Store makeDefault()
    {
        static Store store;
        static bool initialized = false;

        if (initialized)
        {
            return store;
        }

        std::unordered_set<std::string> enabledExtensions;

        if (std::filesystem::exists(
                R"(C:\Program Files (x86)\FRC Driver Station\DriverStation.exe)"))
        {
            enabledExtensions = {"halsim_ds_socket"};
        }
        else
        {
            enabledExtensions = {"halsim_gui"};
        }

        store = {
            // General

            .showMainMenu = true,
            .showExitWarning = true,
            .launchRobotCode = true,
            .launchElastic = true,
            .enableDiscordSDK = true,
            .fullscreen = false,

            // Game Specific

            .gameTeam = 6731,
            .gameTeamPool = {151, 69, 97, 4169, 246},
            .gameMatchType = 2,
            .gameMatchNumber = 42,
            .gameMatchTotal = 76,

            .rebuilt2026 = {.energizedRPThreshold = 100, .superchargedRPThreshold = 360},

            .viewMode = CameraView::Field,
            .cameraFov = 60.0f,
            .cameraTarget = {0, 0},

            // Simulation

            .enabledExtensions = enabledExtensions,
            .jvmArguments = {},
            .codeArguments = {},
            .javaLogMaxBytes = 1024ull * 1024ull * 1024ull, // 1 GB
            .ntPeriodic = 0.022,
            .enableFrameInterpolation = true,

            // Graphics

            .renderApi = "auto",
            .enableVSync = true,
            .updateWhileMinimized = true,
            .useFullDetailRobotModel = RobotModelSimplificationMode::Distance,
            .cacheModels = true,
            .writeObjectMotionVectors = true,
            .enableGTAO = true,
            .enableTAA = true,
            .enableMotionBlur = true,
            .enableBloom = true,
            .enableDebugMenu = false,
        };

        initialized = true;
        return store;
    }

    void loadDefaultSettings()
    {
        logger->info("Loading default settings.");
        current = makeDefault();
    }

    // NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables,readability-magic-numbers)

    uint64_t parseHumanSizeToBytes(const std::string &sizeStr)
    {
        constexpr uint64_t MULTIPLIER_KB = 1024ull;
        constexpr uint64_t MULTIPLIER_MB = 1024ull * MULTIPLIER_KB;
        constexpr uint64_t MULTIPLIER_GB = 1024ull * MULTIPLIER_MB;
        constexpr uint64_t MULTIPLIER_TB = 1024ull * MULTIPLIER_GB;

        size_t offset = 0;
        const double number = std::stod(sizeStr, &offset);
        uint64_t multiplier = 1;

        while (offset < sizeStr.size() &&
               std::isspace(static_cast<unsigned char>(sizeStr.at(offset))) != 0)
        {
            offset++;
        }

        std::string unitPart;
        while (offset < sizeStr.size() &&
               std::isalpha(static_cast<unsigned char>(sizeStr.at(offset))) != 0)
        {
            unitPart +=
                static_cast<char>(std::tolower(static_cast<unsigned char>(sizeStr.at(offset))));
            offset++;
        }

        if (unitPart == "k" || unitPart == "kb")
        {
            multiplier = MULTIPLIER_KB;
        }
        else if (unitPart == "m" || unitPart == "mb")
        {
            multiplier = MULTIPLIER_MB;
        }
        else if (unitPart == "g" || unitPart == "gb")
        {
            multiplier = MULTIPLIER_GB;
        }
        else if (unitPart == "t" || unitPart == "tb")
        {
            multiplier = MULTIPLIER_TB;
        }
        else if (!unitPart.empty() && unitPart != "b")
        {
            throw std::invalid_argument("Invalid size unit: " + unitPart);
        }

        const double bytesValue = number * static_cast<double>(multiplier);

        if (!std::isfinite(bytesValue) || bytesValue < 0.0 ||
            bytesValue > static_cast<double>(std::numeric_limits<uint64_t>::max()))
        {
            throw std::out_of_range("Invalid size value");
        }

        return static_cast<uint64_t>(std::llround(bytesValue));
    }

    std::string humanReadableSize(uint64_t bytes)
    {
        constexpr uint64_t MULTIPLIER = 1024ull;
        constexpr std::array<const char *, 5> units = {"B", "KB", "MB", "GB", "TB"};

        auto value = static_cast<double>(bytes);
        int unitIndex = 0;

        while (value >= static_cast<double>(MULTIPLIER) &&
               unitIndex < static_cast<int>(units.size()) - 1)
        {
            value /= static_cast<double>(MULTIPLIER);
            unitIndex++;
        }

        const auto clampedUnitIndex = std::clamp(unitIndex, 0, static_cast<int>(units.size()) - 1);

        if (unitIndex == 0)
        {
            return std::to_string(bytes) + " " + units.at(clampedUnitIndex);
        }

        std::ostringstream valueStream;
        valueStream << std::fixed << std::setprecision(2) << value;
        std::string valueText = valueStream.str();
        while (!valueText.empty() && valueText.back() == '0')
        {
            valueText.pop_back();
        }
        if (!valueText.empty() && valueText.back() == '.')
        {
            valueText.pop_back();
        }

        return valueText + " " + units.at(clampedUnitIndex);
    }

    void loadSettings()
    {
        current = makeDefault();

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

            // General

            current.showMainMenu = json.value("showMainMenu", current.showMainMenu);
            current.showExitWarning = json.value("showExitWarning", current.showExitWarning);
            current.launchRobotCode = json.value("launchRobotCode", current.launchRobotCode);
            current.launchElastic = json.value("launchElastic", current.launchElastic);
            current.enableDiscordSDK = json.value("enableDiscordSDK", current.enableDiscordSDK);
            current.fullscreen = json.value("fullscreen", current.fullscreen);

            // Game Specific

            current.gameTeam = json.value("gameTeam", current.gameTeam);
            current.gameTeamPool = json.value("gameTeamPool", current.gameTeamPool);
            current.gameMatchType = json.value("gameMatchType", current.gameMatchType);
            current.gameMatchNumber = json.value("gameMatchNumber", current.gameMatchNumber);
            current.gameMatchTotal = json.value("gameMatchTotal", current.gameMatchTotal);

            current.rebuilt2026 = json.value("rebuilt2026", current.rebuilt2026);

            current.viewMode = json.value("viewMode", current.viewMode);
            current.cameraFov = json.value("cameraFov", current.cameraFov);
            current.cameraTarget = json.value("cameraTarget", current.cameraTarget);

            // Simulation

            current.enabledExtensions = json.value("enabledExtensions", current.enabledExtensions);
            current.jvmArguments = json.value("jvmArguments", current.jvmArguments);
            current.codeArguments = json.value("codeArguments", current.codeArguments);
            current.javaLogMaxBytes = parseHumanSizeToBytes(
                json.value("javaLogMaxBytes", humanReadableSize(current.javaLogMaxBytes)));
            current.ntPeriodic = json.value("ntPeriodic", current.ntPeriodic);
            current.enableFrameInterpolation =
                json.value("enableFrameInterpolation", current.enableFrameInterpolation);

            // Graphics

            current.renderApi = json.value("renderApi", current.renderApi);
            current.enableVSync = json.value("enableVSync", current.enableVSync);
            current.updateWhileMinimized =
                json.value("updateWhileMinimized", current.updateWhileMinimized);
            current.useFullDetailRobotModel =
                json.value("useFullDetailRobotModel", current.useFullDetailRobotModel);
            current.cacheModels = json.value("cacheModels", current.cacheModels);
            current.writeObjectMotionVectors =
                json.value("writeObjectMotionVectors", current.writeObjectMotionVectors);
            current.enableGTAO = json.value("enableGTAO", current.enableGTAO);
            current.enableTAA = json.value("enableTAA", current.enableTAA);
            current.enableMotionBlur = json.value("enableMotionBlur", current.enableMotionBlur);
            current.enableBloom = json.value("enableBloom", current.enableBloom);
            current.enableDebugMenu = json.value("enableDebugMenu", current.enableDebugMenu);

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

        // General

        json["showMainMenu"] = current.showMainMenu;
        json["showExitWarning"] = current.showExitWarning;
        json["launchRobotCode"] = current.launchRobotCode;
        json["launchElastic"] = current.launchElastic;
        json["enableDiscordSDK"] = current.enableDiscordSDK;
        json["fullscreen"] = current.fullscreen;

        // Game Specific

        json["gameTeam"] = current.gameTeam;
        json["gameTeamPool"] = current.gameTeamPool;
        json["gameMatchType"] = current.gameMatchType;
        json["gameMatchNumber"] = current.gameMatchNumber;
        json["gameMatchTotal"] = current.gameMatchTotal;

        json["rebuilt2026"] = current.rebuilt2026;

        json["viewMode"] = current.viewMode;
        json["cameraFov"] = current.cameraFov;
        json["cameraTarget"] = current.cameraTarget;

        // Simulation

        json["enabledExtensions"] = current.enabledExtensions;
        json["jvmArguments"] = current.jvmArguments;
        json["codeArguments"] = current.codeArguments;
        json["javaLogMaxBytes"] = humanReadableSize(current.javaLogMaxBytes);
        json["ntPeriodic"] = current.ntPeriodic;
        json["enableFrameInterpolation"] = current.enableFrameInterpolation;

        // Graphics

        json["renderApi"] = current.renderApi;
        json["enableVSync"] = current.enableVSync;
        json["updateWhileMinimized"] = current.updateWhileMinimized;
        json["useFullDetailRobotModel"] = current.useFullDetailRobotModel;
        json["cacheModels"] = current.cacheModels;
        json["writeObjectMotionVectors"] = current.writeObjectMotionVectors;
        json["enableGTAO"] = current.enableGTAO;
        json["enableTAA"] = current.enableTAA;
        json["enableMotionBlur"] = current.enableMotionBlur;
        json["enableBloom"] = current.enableBloom;
        json["enableDebugMenu"] = current.enableDebugMenu;

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