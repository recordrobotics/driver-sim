#pragma once

#include "storedasset.h"

#include <utility>

class RemoteStoredAsset : public StoredAsset
{
    std::string remoteUrl;
    mz_zip_archive zip;

  public:
    RemoteStoredAsset(const std::string &relativeExtractPath, const std::string &hash,
                      const std::string &sdlPrefPath, std::string_view url)
        : StoredAsset(relativeExtractPath, "", hash, sdlPrefPath, "remote"), remoteUrl(url)
    {
    }

  protected:
    mz_zip_archive *performDownload(std::stop_token stoken) override;
    void cleanup() override;
};