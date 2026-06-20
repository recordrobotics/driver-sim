#include "storedasset.h"

#include <blackboard_app/logger.h>
#include <miniz.h>
#include <fstream>
#include <vector>

using namespace blackboard::logger;

namespace fs = std::filesystem;

void StoredAsset::setError(const std::string &err)
{
    logger->error("Asset error: {}", err);
    std::lock_guard<std::mutex> lock(errorMutex);
    errorMessage = err;
    state = AssetState::Error;
}

std::string StoredAsset::readSha256(const fs::path &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        logger->error("Failed to open hash file: {}", path.string());
        return "";
    }

    std::string hash;
    std::getline(file, hash);
    logger->info("Read hash from {}: {}", path.string(), hash);
    return hash;
}

bool isFilenameSafe(const std::string &name)
{
    const std::string illegalChars = "*?<>|:\"\\/";
    if (name.find_first_of(illegalChars) != std::string::npos)
        return false;

    // reserved windows names
    std::string baseName = fs::path(name).stem().string();
    const std::vector<std::string> reserved = {"CON", "PRN", "AUX", "NUL", "COM1", "LPT1"};
    for (const auto &r : reserved)
    {
        if (baseName == r)
            return false;
    }
    return true;
}

void StoredAsset::extractZip(const fs::path &zipPath, const fs::path &extractTo)
{
    logger->info("Extracting zip file {} to {}", zipPath.string(), extractTo.string());
    mz_zip_archive zip_archive;
    memset(&zip_archive, 0, sizeof(zip_archive));

    if (!mz_zip_reader_init_file(&zip_archive, zipPath.string().c_str(), 0))
    {
        setError("Failed to open zip file: " + zipPath.string());
        return;
    }

    mz_uint num_files = mz_zip_reader_get_num_files(&zip_archive);
    logger->trace("Zip archive contains {} files", num_files);

    for (mz_uint i = 0; i < num_files; i++)
    {
        progressPercent = static_cast<int>((i * 100) / num_files);
        mz_zip_archive_file_stat file_stat;
        if (!mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
        {
            logger->trace("Failed to get file stat for index {}: {}", i, mz_zip_get_error_string(zip_archive.m_last_error));
            continue;
        }

        fs::path baseDir = fs::weakly_canonical(extractTo);
        fs::path targetPath = fs::weakly_canonical(extractTo / file_stat.m_filename);
        auto [mismatch_base, mismatch_target] = std::mismatch(baseDir.begin(), baseDir.end(), targetPath.begin());

        if (mismatch_base != baseDir.end())
        {
            logger->error("Invalid path detected in file {}", file_stat.m_filename);
            continue;
        }

        if (!isFilenameSafe(targetPath.filename().string()))
        {
            logger->error("Illegal filename '{}' detected.", targetPath.filename().string());
            continue;
        }

        fs::path outputPath = extractTo / file_stat.m_filename;

        if (mz_zip_reader_is_file_a_directory(&zip_archive, i))
        {
            logger->trace("Creating directory: {}", outputPath.string());
            if (fs::exists(outputPath) && fs::is_directory(outputPath))
            {
                fs::path dirName = outputPath.has_filename() ? outputPath.filename() : outputPath.parent_path().filename();
                if (std::find(cleanReplaceFolders.begin(), cleanReplaceFolders.end(), dirName.string()) != cleanReplaceFolders.end())
                {
                    logger->trace("Cleaning directory: {}", outputPath.string());
                    fs::remove_all(outputPath);
                }
            }
            fs::create_directories(outputPath);
            continue;
        }

        fs::create_directories(outputPath.parent_path());

        if (!mz_zip_reader_extract_to_file(&zip_archive, i, outputPath.string().c_str(), 0))
        {
            mz_zip_reader_end(&zip_archive);
            setError("Failed to extract file: " + std::string(file_stat.m_filename));
            return;
        }

        logger->trace("Extracted file: {}", outputPath.string());
    }

    progressPercent = 100;

    mz_zip_reader_end(&zip_archive);
    state = AssetState::Cleanup;

    logger->info("Extraction complete for {}", zipPath.string());
}

void StoredAsset::cleanupExtractedFiles()
{
    try
    {
        logger->info("Cleaning up temporary zip file: {}", localTempZipPath.string());
        fs::remove(localTempZipPath);
        state = AssetState::Complete;
    }
    catch (const std::exception &e)
    {
        setError("Failed to clean up temporary files: " + std::string(e.what()));
    }
}

StoredAsset::StoredAsset(const std::string &relativeExtractPath, const std::string &hash, const std::string &sdlPrefPath)
    : expectedSha256(hash)
{
    localExtractPath = fs::path(sdlPrefPath) / relativeExtractPath;
    localHashPath = fs::path(sdlPrefPath) / (relativeExtractPath + ".sha256");
    localTempZipPath = fs::path(sdlPrefPath) / (relativeExtractPath + ".tmp.zip");
}

StoredAsset::~StoredAsset()
{
    if (workerThread.joinable())
    {
        workerThread.request_stop();
    }
}

void StoredAsset::verifyOrDownload()
{
    if (state != AssetState::Idle && state != AssetState::Error)
        return;

    workerThread = std::jthread([this](std::stop_token stoken)
                                {
            state = AssetState::Verifying;
            progressPercent = 0;

            if (fs::exists(localHashPath)) {
                std::string actualHash = readSha256(localHashPath);
                if (actualHash == expectedSha256) {
                    progressPercent = 100;
                    state = AssetState::Complete;
                    logger->info("Asset is valid and up to date: {}", localExtractPath.string());
                    quickLoaded = true;
                    return;
                }
            }

            if (stoken.stop_requested()) return;

            fs::create_directories(localExtractPath.parent_path());
            performDownload(stoken);

            if(state == AssetState::Extracting) {
                if(stoken.stop_requested()) return;

                extractZip(localTempZipPath, localExtractPath);
                cleanupExtractedFiles();

                // Write hash file
                std::ofstream hashFile(localHashPath, std::ios::trunc);
                if (hashFile)            {
                    hashFile << expectedSha256;
                }
                else
                {
                    setError("Failed to write hash file for asset.");
                }
            } });
}

std::string StoredAsset::getError()
{
    std::lock_guard<std::mutex> lock(errorMutex);
    return errorMessage;
}