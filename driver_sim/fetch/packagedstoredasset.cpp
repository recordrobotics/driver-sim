#include "packagedstoredasset.h"

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace fs = std::filesystem;

ArchiveData PackagedStoredAsset::performDownload(std::stop_token stoken)
{
    state = AssetState::Extracting;

    struct archive *reader = archive_read_new();

    archive_read_support_filter_all(reader);
    archive_read_support_format_all(reader);

    if (archive_read_open_memory(reader, archiveBytes.data(), archiveBytes.size()) != ARCHIVE_OK)
    {
        const char *errorMsg = archive_error_string(reader);
        logger->error("Error loading manifest: Failed to initialize archive reader {}",
                      errorMsg != nullptr ? errorMsg : "unknown error");

        archive_read_free(reader);
        return {.archive = nullptr, .archiveSize = 0};
    }

    archiveData.archive = reader;
    archiveData.archiveSize = static_cast<uint32_t>(archiveBytes.size());

    return archiveData;
}

void PackagedStoredAsset::cleanup()
{
    // No cleanup needed for packaged assets
}