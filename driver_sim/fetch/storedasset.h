#pragma once

#include <atomic>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

enum class AssetState : uint8_t
{
    Idle,
    Verifying,
    Downloading,
    Writing,
    Extracting,
    Cleanup,
    Complete,
    Error
};

class StoredAsset
{
  protected:
    std::filesystem::path localExtractPath;
    std::filesystem::path localHashPath;
    std::filesystem::path localTempZipPath;
    std::string expectedSha256;

    std::atomic<AssetState> state{AssetState::Idle};
    std::atomic<int> progressPercent{0};

    std::string errorMessage;
    std::mutex errorMutex;

    std::jthread workerThread;

    bool quickLoaded = false; // for assets already valid on disk

    virtual void performDownload(std::stop_token stoken) = 0;

    void setError(const std::string &err);

    static std::string readSha256(const std::filesystem::path &path);

    void deleteOldFiles(const std::filesystem::path &rootFolder);
    void extractZip(const std::filesystem::path &zipPath, const std::filesystem::path &extractTo);
    void cleanupExtractedFiles();

  public:
    StoredAsset(const std::string &relativeExtractPath, std::string hash,
                const std::string &sdlPrefPath, const std::string &sourceType);

    virtual ~StoredAsset();

    StoredAsset(const StoredAsset &) = delete;
    StoredAsset &operator=(const StoredAsset &) = delete;
    StoredAsset(StoredAsset &&) = delete;
    StoredAsset &operator=(StoredAsset &&) = delete;

    void verifyOrDownload();

    int getProgress() const { return progressPercent.load(); }
    AssetState getState() const { return state.load(); }
    std::string getError();
    bool isQuickLoaded() const { return quickLoaded; }

    std::vector<std::string> keepPaths; // folders/files to keep when deleting old files

    static std::vector<std::string> &getCreatedAssets();
};