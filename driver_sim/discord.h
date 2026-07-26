#pragma once

#include <discordsdk/discord.h>
#include "fetch/storedasset.h"
#include "fetch/remotestoredasset.h"

class Discord
{
public:
    Discord();

    Discord(const Discord &) = delete;
    Discord &operator=(const Discord &) = delete;
    Discord(Discord &&) noexcept = default;
    Discord &operator=(Discord &&) noexcept = default;

    bool isAvailable() const;

    void update();

    void setMenu();
    void setField(
        int gameYear,
        int allianceStation,
        int driverScore,
        int opponentScore,
        const std::string &robotName,
        const std::string &driveMode,
        const std::string &robotRepoUrl,
        const std::string &robotDownloadUrl,
        uint64_t matchEndTimeMs);

private:
    void onLoaded();
    void updateActivity(const discordpp::Activity &activity);

    uint64_t startTimeMs = 0;
    bool loaded = false;
    bool available = false;
    std::shared_ptr<discordpp::Client> client;
    std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> discordSDKAsset;
};