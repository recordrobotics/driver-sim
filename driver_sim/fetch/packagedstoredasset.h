#pragma once

#include "storedasset.h"

#include <cstdint>
#include <span>
#include <utility>

class PackagedStoredAsset : public StoredAsset
{
    ArchiveData archiveData;
    std::span<const uint8_t> archiveBytes;

  public:
    PackagedStoredAsset(const std::string &relativeExtractPath, const std::string &hash,
                        const std::string &sdlPrefPath,
                        const std::span<const uint8_t> &archiveBytes)
        : StoredAsset(relativeExtractPath,
                      relativeExtractPath /* manifest archive stores with same name */, hash,
                      sdlPrefPath, "packaged"),
          archiveData({.archive = nullptr, .archiveSize = 0}), archiveBytes(archiveBytes)
    {
    }

  protected:
    ArchiveData performDownload(std::stop_token stoken) override;
    void cleanup() override;
};