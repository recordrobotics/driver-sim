#pragma once

#include "storedasset.h"

class PackagedStoredAsset : public StoredAsset
{
    std::span<const uint8_t> embeddedData;

  public:
    PackagedStoredAsset(const std::string &relativeExtractPath, const std::string &hash,
                        const std::string &sdlPrefPath, std::span<const uint8_t> data)
        : StoredAsset(relativeExtractPath, hash, sdlPrefPath, "packaged"), embeddedData(data)
    {
    }

  protected:
    void performDownload(std::stop_token stoken) override;
};