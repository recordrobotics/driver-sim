#pragma once

#include "storedasset.h"

#include <utility>

class RemoteStoredAsset : public StoredAsset
{
    std::string remoteUrl;

  public:
    RemoteStoredAsset(const std::string &relativeExtractPath, const std::string &hash,
                      const std::string &sdlPrefPath, std::string url)
        : StoredAsset(relativeExtractPath, hash, sdlPrefPath, "remote"), remoteUrl(std::move(url))
    {
    }

  protected:
    void performDownload(std::stop_token stoken) override;
};