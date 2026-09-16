#include "remotestoredasset.h"

#include <blackboard_app/logger.h>
#include <cpr/cpr.h>
#include <fstream>

using namespace blackboard::logger;
namespace fs = std::filesystem;

mz_zip_archive *RemoteStoredAsset::performDownload(std::stop_token stoken)
{
    logger->info("Starting download of asset from URL: {}", remoteUrl);
    state = AssetState::Downloading;

    std::ofstream ofs(localTempZipPath, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        setError("Could not open local file for writing: " + localTempZipPath.string());
        return nullptr;
    }

    cpr::Response resp = cpr::Download(
        ofs, cpr::Url{remoteUrl},
        cpr::ProgressCallback(
            [this, &stoken](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
                            cpr::cpr_off_t /*uploadTotal*/, cpr::cpr_off_t /*uploadNow*/,
                            intptr_t /*userdata*/) -> bool
            {
                if (stoken.stop_requested())
                {
                    logger->info("Download cancelled by engine during progress callback: {}",
                                 remoteUrl);
                    return false;
                }

                if (downloadTotal > 0)
                {
                    progressPercent = static_cast<int>((downloadNow * 100) / downloadTotal);
                    logger->trace("Download progress: {}% ({} / {})", progressPercent.load(),
                                  downloadNow, downloadTotal);
                }

                return true; // Continue downloading
            }));

    ofs.close();

    if (stoken.stop_requested())
    {
        logger->info("Download cancelled by engine: {}", remoteUrl);
        std::filesystem::remove(localTempZipPath); // Delete the partial file
        return nullptr;
    }

    if (resp.status_code == 200)
    {
        progressPercent = 100;
        state = AssetState::Extracting;
        logger->info("Download complete for URL: {}", remoteUrl);
    }
    else
    {
        logger->error("Failed to download asset from {}: HTTP {} - {}", remoteUrl, resp.status_code,
                      resp.error.message);
        std::filesystem::remove(localTempZipPath); // Delete potential files

        if (resp.error.code != cpr::ErrorCode::OK)
        {
            setError("Network Error: " + resp.error.message + " (" + remoteUrl + ")");
        }
        else
        {
            setError("HTTP Error " + std::to_string(resp.status_code) + " from: " + remoteUrl);
        }
        return nullptr;
    }

    logger->info("Extracting zip file {} to {}", localTempZipPath.string(),
                 localExtractPath.string());

    memset(&zip, 0, sizeof(zip));

    if (mz_zip_reader_init_file(&zip, localTempZipPath.string().c_str(), 0) == 0)
    {
        setError("Failed to open zip file: " + localTempZipPath.string());
        return nullptr;
    }
    return &zip;
}

void RemoteStoredAsset::cleanup()
{
    try
    {
        logger->info("Cleaning up temporary zip file: {}", localTempZipPath.string());
        mz_zip_reader_end(&zip);
        fs::remove(localTempZipPath);
    }
    catch (const std::exception &e)
    {
        setError("Failed to clean up temporary files: " + std::string(e.what()));
    }
}