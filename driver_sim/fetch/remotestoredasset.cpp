#include "remotestoredasset.h"

#include <blackboard_app/logger.h>
#include <cpr/cpr.h>
#include <fstream>

using namespace blackboard::logger;

void RemoteStoredAsset::performDownload(std::stop_token stoken)
{
    logger->info("Starting download of asset from URL: {}", remoteUrl);
    state = AssetState::Downloading;

    std::ofstream ofs(localTempZipPath, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        setError("Could not open local file for writing: " + localTempZipPath.string());
        return;
    }

    cpr::Response r = cpr::Download(
        ofs,
        cpr::Url{remoteUrl},
        cpr::ProgressCallback([this, &stoken](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
                                              cpr::cpr_off_t /*uploadTotal*/, cpr::cpr_off_t /*uploadNow*/,
                                              intptr_t /*userdata*/) -> bool
                              {
                                  if (stoken.stop_requested())
                                  {
                                      logger->info("Download cancelled by engine during progress callback: {}", remoteUrl);
                                      return false;
                                  }

                                  if (downloadTotal > 0)
                                  {
                                      progressPercent = static_cast<int>((downloadNow * 100) / downloadTotal);
                                      logger->trace("Download progress: {}% ({} / {})", progressPercent.load(), downloadNow, downloadTotal);
                                  }

                                  return true; // Continue downloading
                              }));

    ofs.close();

    if (stoken.stop_requested())
    {
        logger->info("Download cancelled by engine: {}", remoteUrl);
        std::filesystem::remove(localTempZipPath); // Delete the partial file
        return;
    }

    if (r.status_code == 200)
    {
        progressPercent = 100;
        state = AssetState::Extracting;
        logger->info("Download complete for URL: {}", remoteUrl);
    }
    else
    {
        logger->error("Failed to download asset from {}: HTTP {} - {}", remoteUrl, r.status_code, r.error.message);
        std::filesystem::remove(localTempZipPath); // Delete potential files

        if (r.error.code != cpr::ErrorCode::OK)
        {
            setError("Network Error: " + r.error.message + " (" + remoteUrl + ")");
        }
        else
        {
            setError("HTTP Error " + std::to_string(r.status_code) + " from: " + remoteUrl);
        }
    }
}