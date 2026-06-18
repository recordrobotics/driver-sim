#include "settingsstore.h"

#include <bx/file.h>
#include <bx/error.h>
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace settings
{
    bool enableTAA = true;
    bool enableMotionBlur = true;
    bool writeObjectMotionVectors = false;

    bool showSelectPage = true;
    bool cacheModels = true;
    std::string extraArguments;

    std::unordered_set<std::string> enabledExtensions;

    void loadDefaultSettings()
    {
        logger->info("Loading default settings.");
        enableTAA = true;
        enableMotionBlur = true;
        writeObjectMotionVectors = false;

        showSelectPage = true;
        cacheModels = true;
        extraArguments = "";

        enabledExtensions = {"halsim_ds_socket"};
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
            writeObjectMotionVectors = j.value("writeObjectMotionVectors", writeObjectMotionVectors);

            showSelectPage = j.value("showSelectPage", showSelectPage);
            cacheModels = j.value("cacheModels", cacheModels);
            extraArguments = j.value("extraArguments", extraArguments);
            enabledExtensions = j.value("enabledExtensions", enabledExtensions);
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
        j["writeObjectMotionVectors"] = writeObjectMotionVectors;

        j["showSelectPage"] = showSelectPage;
        j["cacheModels"] = cacheModels;
        j["extraArguments"] = extraArguments;
        j["enabledExtensions"] = enabledExtensions;
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