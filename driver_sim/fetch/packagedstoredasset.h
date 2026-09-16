#pragma once

#include "storedasset.h"
#include <miniz.h>

class PackagedStoredAsset : public StoredAsset
{
    std::shared_ptr<mz_zip_archive> zip;

  public:
    PackagedStoredAsset(const std::string &relativeExtractPath, const std::string &hash,
                        const std::string &sdlPrefPath, std::shared_ptr<mz_zip_archive> zip)
        : StoredAsset(relativeExtractPath,
                      relativeExtractPath /* manifest zip stores with same name */, hash,
                      sdlPrefPath, "packaged"),
          zip(zip)
    {
    }

  protected:
    mz_zip_archive *performDownload(std::stop_token stoken) override;
    void cleanup() override;
};