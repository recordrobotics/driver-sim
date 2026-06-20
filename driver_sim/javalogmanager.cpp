#include "javalogmanager.h"
#include <blackboard_app/logger.h>
#include <SDL3/SDL.h>

#include "settings/settingsstore.h"

using namespace blackboard::logger;

static java_log_manager::UsageStats lastUsageStats{};

void java_log_manager::enforceFolderLimits()
{
    UsageStats stats{};
    stats.maxAllowedBytes = settings::javaLogMaxBytes;
    std::string javaLogPath = std::string(SDL_GetPrefPath(NULL, "DriverSim")) + "code/logs/";

    if (!std::filesystem::exists(javaLogPath))
    {
        logger->info("Java log folder does not exist at {}, skipping log management.", javaLogPath);
        lastUsageStats = stats;
        return;
    }

    std::vector<std::filesystem::directory_entry> logFiles;
    for (const auto &entry : std::filesystem::directory_iterator(javaLogPath))
    {
        if (entry.is_regular_file())
        {
            uint64_t fileSize = entry.file_size();
            stats.totalBytes += fileSize;
            stats.totalFiles++;

            if (fileSize > stats.largestFileBytes)
            {
                stats.largestFileBytes = fileSize;
            }

            auto lastWriteTime = std::filesystem::last_write_time(entry);
            auto ageSeconds = std::chrono::duration_cast<std::chrono::seconds>(std::filesystem::file_time_type::clock::now() - lastWriteTime).count();
            if (ageSeconds > stats.oldestFileAgeSeconds)
            {
                stats.oldestFileAgeSeconds = ageSeconds;
            }

            logFiles.push_back(entry);
        }
    }

    logger->info("Java log folder usage: {} bytes across {} files. Oldest file age: {} seconds. Largest file size: {} bytes. Max allowed bytes: {}",
                 stats.totalBytes, stats.totalFiles, stats.oldestFileAgeSeconds, stats.largestFileBytes, stats.maxAllowedBytes);

    if (stats.totalBytes > stats.maxAllowedBytes)
    {
        logger->warn("Java log folder size {} bytes exceeds the maximum allowed {} bytes. Deleting oldest files until under limit.",
                     stats.totalBytes, stats.maxAllowedBytes);

        // sort by last write time, oldest first
        std::sort(logFiles.begin(), logFiles.end(),
                  [](const std::filesystem::directory_entry &a, const std::filesystem::directory_entry &b)
                  {
                      return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
                  });

        uint64_t bytesDeleted = 0;
        for (const auto &file : logFiles)
        {
            uint64_t fileSize = file.file_size();
            std::error_code ec;
            std::filesystem::remove(file.path(), ec);
            if (!ec)
            {
                bytesDeleted += fileSize;
                logger->info("Deleted Java log file {} of size {} bytes", file.path().string(), fileSize);
            }
            else
            {
                logger->error("Failed to delete Java log file {}: {}", file.path().string(), ec.message());
            }

            if (stats.totalBytes - bytesDeleted <= stats.maxAllowedBytes)
            {
                break;
            }
        }
    }

    lastUsageStats = stats;
}

java_log_manager::UsageStats java_log_manager::getUsageStats()
{
    return lastUsageStats;
}