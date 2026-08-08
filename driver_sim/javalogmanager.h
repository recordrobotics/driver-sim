#pragma once

#include <cstdint>

namespace java_log_manager
{
    struct UsageStats
    {
        uint64_t totalBytes;
        uint64_t totalFiles;
        uint64_t oldestFileAgeSeconds;
        uint64_t largestFileBytes;
        uint64_t maxAllowedBytes;
    };

    void enforceFolderLimits();
    UsageStats getUsageStats();
}