#pragma once

#include "storedasset.h"

class RemoteStoredAsset : public StoredAsset
{
    std::string remoteUrl;

public:
    RemoteStoredAsset(const std::string &relativeExtractPath, const std::string &hash, const std::string &sdlPrefPath, const std::string &url)
        : StoredAsset(relativeExtractPath, hash, sdlPrefPath), remoteUrl(url) {}

protected:
    void performDownload(std::stop_token stoken) override;
};