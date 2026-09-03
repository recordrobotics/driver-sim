#pragma once

#include "fetch/remotestoredasset.h"
#include "fetch/storedasset.h"
#include <discordsdk/discord.h>

class Discord
{
  public:
    Discord();
    ~Discord() = default;

    Discord(const Discord &) = delete;
    Discord &operator=(const Discord &) = delete;
    Discord(Discord &&) noexcept = default;
    Discord &operator=(Discord &&) noexcept = default;

    [[nodiscard]] bool isAvailable() const;

    void update();

    void setMenu();
    void setField(const std::string &gameYear, int allianceStation, int driverScore,
                  int opponentScore, const std::string &robotName, const std::string &driveMode,
                  const std::string &robotRepoUrl, const std::string &robotDownloadUrl,
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