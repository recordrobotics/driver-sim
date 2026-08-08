#pragma once

#include <atomic>
#include <bimg/decode.h>
#include <blackboard_app/gui.h>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>

enum class LogoState : uint8_t
{
    NotLoaded,
    Loading,
    Loaded,
    Error
};

class TeamLogoCache
{
  public:
    TeamLogoCache();
    ~TeamLogoCache();

    TeamLogoCache(const TeamLogoCache &) = delete;
    TeamLogoCache &operator=(const TeamLogoCache &) = delete;
    TeamLogoCache(TeamLogoCache &&) noexcept = default;
    TeamLogoCache &operator=(TeamLogoCache &&) noexcept = default;

    blackboard::gui::ImTexture getTeamLogo(int teamNumber);
    void update();

  private:
    std::filesystem::path logoCacheDirectory;

    std::unordered_map<int, blackboard::gui::ImTexture> logoCache;
    std::unordered_map<int, std::atomic<LogoState>> logoStates;

    std::mutex processQueueMutex;
    std::queue<std::pair<int, bimg::ImageContainer *>> processQueue;

    std::unordered_map<int, std::jthread> workerThreads;

    blackboard::gui::ImTexture placeholderTexture;
};