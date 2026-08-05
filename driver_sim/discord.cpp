#include "discord.h"
#include "settings/settingsstore.h"
#include <SDL3/SDL.h>
#include <blackboard_app/logger.h>

static constexpr uint64_t APPLICATION_ID = 1530529364221890771;

using namespace blackboard::logger;

Discord::Discord()
{
    std::string prefPath = SDL_GetPrefPath(NULL, "DriverSim");

    discordSDKAsset = std::make_unique<RemoteStoredAsset>(
        "discord_sdk", "2a7c8b043ca04a14a10c64b4f1116fe2a93bb6f6f4f0b4784c0ca1fc06ca832e", prefPath,
        "https://hamster1.ddns.net/"
        "discord_sdk-2a7c8b043ca04a14a10c64b4f1116fe2a93bb6f6f4f0b4784c0ca1fc06ca832e.zip");

    discordpp::SetLibrarySearchPath(prefPath + "discord_sdk");

    startTimeMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch())
                                            .count());

    if (settings::enableDiscordSDK)
    {
        discordSDKAsset->verifyOrDownload();
    }
}

bool Discord::isAvailable() const { return available; }

void Discord::update()
{
    if (!loaded && discordSDKAsset->getState() == AssetState::Complete)
    {
        onLoaded();
        loaded = true;
    }

    if (available)
    {
        discordpp::RunCallbacks();
    }
}

void Discord::onLoaded()
{
    available = discordpp::IsAvailable();

    if (!available)
    {
        logger->warn("Discord SDK is not available. Discord features will be disabled.");
        return;
    }

    client = std::make_shared<discordpp::Client>();
    client->SetApplicationId(APPLICATION_ID);

    setMenu();
}

void Discord::updateActivity(const discordpp::Activity &activity)
{
    if (available)
    {
        client->UpdateRichPresence(
            activity,
            [](discordpp::ClientResult result)
            {
                if (!result.Successful())
                {
                    logger->error("Failed to update Discord rich presence: {0}", result.Error());
                }
            });
    }
}

void Discord::setMenu()
{
    if (!available)
    {
        return;
    }

    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);

    activity.SetName("Driver Sim");
    activity.SetState("In Menu");

    discordpp::ActivityTimestamps timestamps;
    timestamps.SetStart(startTimeMs);
    activity.SetTimestamps(timestamps);

    discordpp::ActivityButton viewButton;
    viewButton.SetLabel("View on GitHub");
    viewButton.SetUrl("https://github.com/recordrobotics/driver-sim");
    activity.AddButton(viewButton);

    updateActivity(activity);
}

void Discord::setField(int gameYear, int allianceStation, int driverScore, int opponentScore,
                       const std::string &robotName, const std::string &driveMode,
                       const std::string &robotRepoUrl, const std::string &robotDownloadUrl,
                       uint64_t matchEndTimeMs)
{
    if (!available)
    {
        return;
    }

    discordpp::Activity activity;
    activity.SetType(discordpp::ActivityTypes::Playing);

    discordpp::ActivityAssets assets;

    if (gameYear == 2026)
    {
        assets.SetLargeImage("2026");
        assets.SetLargeText("2026 Rebuilt");
    }

    int dsIndex = allianceStation < 1 ? 1 : (allianceStation - 1) % 3 + 1;
    assets.SetSmallImage(
        (allianceStation == 1 || allianceStation == 2 || allianceStation == 3) ? "red" : "blue");
    assets.SetSmallText((allianceStation == 1 || allianceStation == 2 || allianceStation == 3)
                            ? "Red Alliance (Red " + std::to_string(dsIndex) + ")"
                            : "Blue Alliance (Blue " + std::to_string(dsIndex) + ")");

    activity.SetAssets(assets);

    activity.SetName("Driver Sim");
    activity.SetDetails("Score: " + std::to_string(driverScore) + " : " +
                        std::to_string(opponentScore));
    activity.SetState("Driving " + robotName + " (" + driveMode + ")");
    activity.SetStateUrl(robotRepoUrl);
    activity.SetStatusDisplayType(discordpp::StatusDisplayTypes::State);

    discordpp::ActivityTimestamps timestamps;
    timestamps.SetStart(startTimeMs);
    timestamps.SetEnd(matchEndTimeMs);
    activity.SetTimestamps(timestamps);

    discordpp::ActivityButton downloadButton;
    downloadButton.SetLabel("Get Microwave");
    downloadButton.SetUrl(robotDownloadUrl);
    activity.AddButton(downloadButton);

    discordpp::ActivityButton viewButton;
    viewButton.SetLabel("View on GitHub");
    viewButton.SetUrl("https://github.com/recordrobotics/driver-sim");
    activity.AddButton(viewButton);

    updateActivity(activity);
}