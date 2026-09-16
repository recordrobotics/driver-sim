#include "packagedstoredasset.h"

#include <blackboard_app/logger.h>

using namespace blackboard::logger;

namespace fs = std::filesystem;

mz_zip_archive *PackagedStoredAsset::performDownload(std::stop_token stoken)
{
    state = AssetState::Extracting;
    return zip.get();
}

void PackagedStoredAsset::cleanup()
{
    // No cleanup needed for packaged assets
}