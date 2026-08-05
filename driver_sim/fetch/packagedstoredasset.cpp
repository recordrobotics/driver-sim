#include "packagedstoredasset.h"

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace fs = std::filesystem;

void PackagedStoredAsset::performDownload(std::stop_token stoken)
{
    logger->info("Starting write of embedded asset to disk: {}", localTempZipPath.string());
    state = AssetState::Writing;

    std::ofstream outFile(localTempZipPath, std::ios::binary | std::ios::trunc);
    if (!outFile)
    {
        setError("Could not open local filesystem for writing.");
        return;
    }

    const size_t totalBytes = embeddedData.size();
    const size_t chunkSize = 1024 * 64; // 64 kb chunks for progress
    size_t bytesWritten = 0;

    while (bytesWritten < totalBytes)
    {
        if (stoken.stop_requested())
        {
            logger->info("Write cancelled by engine during asset write: {}",
                         localTempZipPath.string());
            outFile.close();
            fs::remove(localTempZipPath); // Cleanup partial file
            return;
        }

        size_t writeSize = std::min(chunkSize, totalBytes - bytesWritten);
        outFile.write(reinterpret_cast<const char *>(embeddedData.data() + bytesWritten),
                      writeSize);

        if (!outFile)
        {
            setError("IO error occurred while writing asset to disk.");
            return;
        }

        bytesWritten += writeSize;
        progressPercent = static_cast<int>((bytesWritten * 100) / totalBytes);
    }

    outFile.close();
    state = AssetState::Extracting;
    logger->info("Finished writing embedded asset to disk: {}", localTempZipPath.string());
}