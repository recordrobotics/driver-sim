#include "remotestoredasset.h"

#include <blackboard_app/logger.h>
#include <cpr/cpr.h>
#include <fstream>

using namespace blackboard::logger;
namespace fs = std::filesystem;

ArchiveData RemoteStoredAsset::performDownload(std::stop_token stoken)
{
    logger->info("Starting download of asset from URL: {}", remoteUrl);
    state = AssetState::Downloading;

    std::ofstream ofs(localTempArchivePath, std::ios::binary | std::ios::trunc);
    if (!ofs)
    {
        setError("Could not open local file for writing: " + localTempArchivePath.string());
        return {.archive = nullptr, .archiveSize = 0};
    }

    int lastLoggedPercent = -1;
    cpr::Response resp = cpr::Download(
        ofs, cpr::Url{remoteUrl},
        cpr::ProgressCallback(
            [this, &stoken,
             &lastLoggedPercent](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
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
                    const int currentPercent =
                        static_cast<int>((downloadNow * 100) / downloadTotal);
                    progressPercent = currentPercent;
                    if (currentPercent != lastLoggedPercent)
                    {
                        lastLoggedPercent = currentPercent;
                        logger->trace("Download progress: {}% ({} / {})", currentPercent,
                                      downloadNow, downloadTotal);
                    }
                }

                return true; // Continue downloading
            }));

    ofs.close();

    if (stoken.stop_requested())
    {
        logger->info("Download cancelled by engine: {}", remoteUrl);
        std::filesystem::remove(localTempArchivePath); // Delete the partial file
        return {.archive = nullptr, .archiveSize = 0};
    }

    if (resp.status_code == 200)
    {
        progressPercent = 100;
        state = AssetState::Extracting;

        auto disposition = resp.header.find("content-disposition");
        if (disposition != resp.header.end())
        {
            std::string filename = disposition->second;
            auto pos = filename.find("filename=");
            if (pos != std::string::npos)
            {
                filename = filename.substr(pos + 9);
                if (!filename.empty() && filename.front() == '"' && filename.back() == '"')
                {
                    filename = filename.substr(1, filename.size() - 2);
                }
                logger->trace("Downloaded file name: {}", filename);
                std::string extension = fs::path(filename).extension().string();
                if (filename.ends_with(".tar.gz"))
                {
                    extension = ".tar.gz";
                }
                fs::path tempPath = localTempArchivePath;
                localTempArchivePath.replace_extension(extension);
                logger->trace("Renaming temporary archive file to: {}",
                              localTempArchivePath.string());
                std::error_code ec;
                fs::rename(tempPath, localTempArchivePath, ec);
                if (ec)
                {
                    logger->error("Failed to rename temporary archive file: {}", ec.message());
                    setError("Failed to rename temporary archive file: " + ec.message());
                    return {.archive = nullptr, .archiveSize = 0};
                }
            }
        }

        logger->info("Download complete for URL: {}", remoteUrl);
    }
    else
    {
        logger->error("Failed to download asset from {}: HTTP {} - {}", remoteUrl, resp.status_code,
                      resp.error.message);
        std::filesystem::remove(localTempArchivePath); // Delete potential files

        if (resp.error.code != cpr::ErrorCode::OK)
        {
            setError("Network Error: " + resp.error.message + " (" + remoteUrl + ")");
        }
        else
        {
            setError("HTTP Error " + std::to_string(resp.status_code) + " from: " + remoteUrl);
        }
        return {.archive = nullptr, .archiveSize = 0};
    }

    logger->info("Extracting archive file {} to {}", localTempArchivePath.string(),
                 localExtractPath.string());

    struct archive *reader = archive_read_new();

    archive_read_support_filter_all(reader);
    archive_read_support_format_all(reader);

    if (archive_read_open_filename(reader, localTempArchivePath.string().c_str(), 10240) !=
        ARCHIVE_OK)
    {
        const char *errorMsg = archive_error_string(reader);
        setError("Failed to open archive: " +
                 std::string(errorMsg != nullptr ? errorMsg : "unknown error"));

        archive_read_free(reader);
        return {.archive = nullptr, .archiveSize = 0};
    }

    archiveData.archive = reader;

    std::error_code ec;
    const auto totalBytes = fs::file_size(localTempArchivePath, ec);

    if (ec || totalBytes == 0)
    {
        logger->warn("Unable to determine archive size {}: {}", ec.message(),
                     localTempArchivePath.string());
        archiveData.archiveSize = 0;
    }
    else
    {
        archiveData.archiveSize = static_cast<uint32_t>(totalBytes);
    }

    return archiveData;
}

void RemoteStoredAsset::cleanup()
{
    try
    {
        logger->info("Cleaning up temporary archive file: {}", localTempArchivePath.string());
        if (archiveData.archive != nullptr)
        {
            archive_read_close(archiveData.archive);
            archive_read_free(archiveData.archive);
            archiveData.archive = nullptr;
        }
        fs::remove(localTempArchivePath);
    }
    catch (const std::exception &e)
    {
        setError("Failed to clean up temporary files: " + std::string(e.what()));
    }
}