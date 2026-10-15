#include "storedasset.h"

#include <algorithm>
#include <archive.h>
#include <archive_entry.h>
#include <blackboard_app/logger.h>
#include <fstream>
#include <ranges>
#include <utility>
#include <vector>

using namespace blackboard::logger;

namespace fs = std::filesystem;

struct PendingLink
{
    fs::path outputPath;
    fs::path targetPath;
    bool symbolic;
};

void StoredAsset::setError(const std::string &err)
{
    logger->error("Asset error: {}", err);
    std::scoped_lock lock(errorMutex);
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
    {
        return false;
    }

    // reserved windows names
    std::string baseName = fs::path(name).stem().string();
    const std::vector<std::string> reserved = {"CON", "PRN", "AUX", "NUL", "COM1", "LPT1"};
    return std::ranges::all_of(reserved, [&baseName](const auto &reservedName)
                               { return baseName != reservedName; });
}

void StoredAsset::deleteOldFiles(const fs::path &rootFolder)
{
    if (!fs::exists(rootFolder) || !fs::is_directory(rootFolder))
    {
        logger->info(
            "Root folder {} does not exist or is not a directory. Skipping deletion of old files.",
            rootFolder.string());
        return;
    }

    logger->info("Deleting old files in {}", rootFolder.string());
    std::vector<fs::path> pathsToDelete;
    for (const auto &entry : fs::recursive_directory_iterator(rootFolder))
    {
        if (entry.is_regular_file() || entry.is_directory())
        {
            fs::path relativePath = fs::relative(entry.path(), rootFolder);
            std::string relativePathStr = relativePath.string();

            // Check if the path is in keepPaths or is a subpath of folder in keepPaths
            bool shouldKeep = false;
            for (const auto &keepPath : keepPaths)
            {
                if (relativePathStr == keepPath || relativePathStr.starts_with(keepPath + "/") ||
                    relativePathStr.starts_with(keepPath + "\\"))
                {
                    shouldKeep = true;
                    break;
                }
            }

            if (!shouldKeep)
            {
                pathsToDelete.push_back(entry.path());
            }
        }
    }

    std::ranges::sort(pathsToDelete,
                      [](const fs::path &compA, const fs::path &compB)
                      {
                          return compA.string().size() >
                                 compB.string().size(); // Sort by path length descending
                      });

    std::error_code error_code;
    for (const auto &path : pathsToDelete)
    {
        if (fs::is_directory(path, error_code))
        {
            fs::remove(path, error_code);
            if (error_code)
            {
                logger->error("Failed to remove directory {}: {}", path.string(),
                              error_code.message());
            }
            else
            {
                logger->trace("Removed directory: {}", path.string());
            }
        }
        else
        {
            fs::remove(path, error_code);
            if (error_code)
            {
                logger->error("Failed to remove file {}: {}", path.string(), error_code.message());
            }
            else
            {
                logger->trace("Removed file: {}", path.string());
            }
        }
    }
}

void StoredAsset::extractArchive(const ArchiveData &archiveData, const fs::path &extractTo)
{
    archive *archive = archiveData.archive;
    uint32_t archiveSize = archiveData.archiveSize;

    if (archive == nullptr)
    {
        setError("Invalid archive reader.");
        return;
    }

    logger->trace("Archive contains {} bytes", archiveSize);

    std::error_code ec;
    fs::create_directories(extractTo, ec);

    if (ec)
    {
        setError("Failed to create extraction directory: " + ec.message());
        return;
    }

    const fs::path baseDir = fs::canonical(extractTo, ec);
    if (ec)
    {
        setError("Failed to resolve extraction directory: " + ec.message());
        return;
    }

    const fs::path archiveRoot = archiveRootDirectory.empty()
                                     ? fs::path{}
                                     : fs::path(archiveRootDirectory).lexically_normal();

    logger->trace("Starting archive extraction to {}", extractTo.string());

    archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;
    std::vector<PendingLink> pendingLinks;

    while ((result = archive_read_next_header(archive, &entry)) == ARCHIVE_OK)
    {
        const char *entryName = archive_entry_pathname(entry);
        if (entryName == nullptr || *entryName == '\0')
        {
            logger->trace("Skipping archive entry with empty filename");
            archive_read_data_skip(archive);
            continue;
        }

        const std::string filename(entryName);
        fs::path filePath(filename);

        // Reject absolute paths
        if (filePath.is_absolute() || filePath.has_root_name())
        {
            logger->error("Invalid absolute archive path: {}", filename);
            archive_read_data_skip(archive);
            continue;
        }

        filePath = filePath.lexically_normal();

        // Match entries under archiveRootDirectory
        if (!archiveRoot.empty())
        {
            const fs::path &normalizedRoot = archiveRoot;

            auto rootIt = normalizedRoot.begin();
            auto fileIt = filePath.begin();

            bool matchesRoot = true;

            for (; rootIt != normalizedRoot.end(); ++rootIt, ++fileIt)
            {
                if (fileIt == filePath.end() || *rootIt != *fileIt)
                {
                    matchesRoot = false;
                    break;
                }
            }

            if (!matchesRoot)
            {
                archive_read_data_skip(archive);
                continue;
            }

            filePath = filePath.lexically_relative(normalizedRoot);
        }

        if (filePath.empty() || filePath == ".")
        {
            archive_read_data_skip(archive);
            continue;
        }

        // Reject traversal components
        bool traversal = false;
        for (const auto &component : filePath)
        {
            if (component == "..")
            {
                traversal = true;
                break;
            }
        }

        if (traversal)
        {
            logger->error("Invalid path detected in file {}", filename);
            archive_read_data_skip(archive);
            continue;
        }

        const fs::path outputPath = (baseDir / filePath).lexically_normal();

        // Ensure destination stays inside baseDir
        auto [baseIt, targetIt] = std::ranges::mismatch(baseDir, outputPath);

        if (baseIt != baseDir.end())
        {
            logger->error("Invalid path detected in file {}", filename);
            archive_read_data_skip(archive);
            continue;
        }

        if (!isFilenameSafe(outputPath.filename().string()))
        {
            logger->error("Illegal filename '{}' detected.", outputPath.filename().string());
            archive_read_data_skip(archive);
            continue;
        }

        const auto type = archive_entry_filetype(entry);

        if (type == AE_IFDIR)
        {
            logger->trace("Creating directory: {}", outputPath.string());

            fs::create_directories(outputPath, ec);
            if (ec)
            {
                setError("Failed to create directory: " + outputPath.string() + ": " +
                         ec.message());
                return;
            }

            archive_read_data_skip(archive);
            continue;
        }

        if (type == AE_IFLNK)
        {
            const char *target = archive_entry_symlink(entry);

            if (target == nullptr || *target == '\0')
            {
                logger->error("Skipping symbolic link with empty target: {}", filename);
                archive_read_data_skip(archive);
                continue;
            }

            fs::path targetPath(target);

            if (targetPath.is_absolute() || targetPath.has_root_name())
            {
                logger->error("Rejecting absolute symbolic-link target: {}", filename);
                archive_read_data_skip(archive);
                continue;
            }

            const fs::path resolvedTarget =
                (outputPath.parent_path() / targetPath).lexically_normal();

            auto [rootIt, targetIt] = std::ranges::mismatch(baseDir, resolvedTarget);

            if (rootIt != baseDir.end())
            {
                logger->error("Rejecting symbolic link outside root: {}", filename);
                archive_read_data_skip(archive);
                continue;
            }

            pendingLinks.push_back({outputPath, targetPath, true});

            archive_read_data_skip(archive);
            continue;
        }

        if (type == AE_IFREG)
        {
            const char *hardlink = archive_entry_hardlink(entry);

            if (hardlink != nullptr && *hardlink != '\0')
            {
                fs::path targetPath(hardlink);

                if (targetPath.is_absolute() || targetPath.has_root_name())
                {
                    logger->error("Rejecting absolute hard-link target: {}", filename);
                    archive_read_data_skip(archive);
                    continue;
                }

                targetPath = targetPath.lexically_normal();

                bool invalidTarget = false;
                for (const auto &component : targetPath)
                {
                    if (component == "..")
                    {
                        invalidTarget = true;
                        break;
                    }
                }

                if (invalidTarget)
                {
                    logger->error("Rejecting unsafe hard-link target: {}", filename);
                    archive_read_data_skip(archive);
                    continue;
                }

                pendingLinks.push_back(
                    {outputPath, (baseDir / targetPath).lexically_normal(), false});

                archive_read_data_skip(archive);
                continue;
            }

            fs::create_directories(outputPath.parent_path(), ec);
            if (ec)
            {
                setError("Failed to create parent directory: " + outputPath.parent_path().string() +
                         ": " + ec.message());
                return;
            }

            // Extract file
            std::ofstream output(outputPath, std::ios::binary | std::ios::trunc);
            if (!output)
            {
                setError("Failed to open output file: " + outputPath.string());
                return;
            }

            std::array<char, 64 * 1024> buffer;
            la_ssize_t bytesRead = 0;

            while ((bytesRead = archive_read_data(archive, buffer.data(), buffer.size())) > 0)
            {
                output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));

                if (!output)
                {
                    setError("Failed to write extracted file: " + outputPath.string());
                    return;
                }

                if (ec || archiveSize == 0)
                {
                    continue;
                }

                const la_int64_t bytesRead = archive_filter_bytes(archive, 0);

                if (bytesRead < 0)
                {
                    continue;
                }

                const auto consumed =
                    std::min<std::uint64_t>(static_cast<std::uint64_t>(bytesRead), archiveSize);

                const int percent = static_cast<int>(consumed * 100ULL / archiveSize);

                progressPercent = std::max(0, std::min(percent, 99));
            }

            if (bytesRead < 0)
            {
                setError("Failed to extract file: " + filename + ": " +
                         archive_error_string(archive));
                return;
            }

            output.close();

            if (!output)
            {
                setError("Failed to close extracted file: " + outputPath.string());
                return;
            }

            const auto permissions = static_cast<fs::perms>(archive_entry_perm(entry) & 0777);

            fs::permissions(outputPath, permissions, fs::perm_options::replace, ec);

            if (ec)
            {
                setError("Failed to set file permissions: " + outputPath.string() + ": " +
                         ec.message());
                return;
            }

            logger->trace("Extracted file: {}", outputPath.string());
        }
        else
        {
            logger->trace("Skipping unsupported archive entry: {}", filename);
            archive_read_data_skip(archive);
            continue;
        }
    }

    if (result != ARCHIVE_EOF)
    {
        const char *errorMsg = archive_error_string(archive);
        setError("Failed while reading archive: " +
                 std::string(errorMsg != nullptr ? errorMsg : "unknown libarchive error"));
        return;
    }

    for (const auto &link : pendingLinks)
    {
        fs::create_directories(link.outputPath.parent_path(), ec);
        if (ec)
        {
            setError("Failed to create link parent directory: " + ec.message());
            return;
        }

        if (link.symbolic)
        {
            fs::create_symlink(link.targetPath, link.outputPath, ec);
        }
        else
        {
            const auto status = fs::symlink_status(link.targetPath, ec);

            if (ec || status.type() != fs::file_type::regular)
            {
                logger->error("Skipping hard link with invalid target: {}",
                              link.targetPath.string());
                ec.clear();
                continue;
            }

            fs::create_hard_link(link.targetPath, link.outputPath, ec);
        }

        if (ec)
        {
            logger->error("Failed to create {} link '{}': {}", link.symbolic ? "symbolic" : "hard",
                          link.outputPath.string(), ec.message());
            ec.clear();
            continue;
        }

        logger->trace("Created {} link: {}", link.symbolic ? "symbolic" : "hard",
                      link.outputPath.string());
    }

    progressPercent = 100;
    state = AssetState::Cleanup;

    logger->info("Extraction complete to {}", extractTo.string());
}

StoredAsset::StoredAsset(const std::string &relativeExtractPath, std::string archiveRootDirectory,
                         std::string hash, const std::string &sdlPrefPath,
                         const std::string &sourceType)
    : archiveRootDirectory(std::move(archiveRootDirectory)), expectedSha256(std::move(hash))
{
    localExtractPath = fs::path(sdlPrefPath) / relativeExtractPath;
    localHashPath = fs::path(sdlPrefPath) / (relativeExtractPath + ".sha256");
    localTempArchivePath = fs::path(sdlPrefPath) / (relativeExtractPath + ".tmp");

    getCreatedAssets().push_back(relativeExtractPath + ":" + expectedSha256 + " (" + sourceType +
                                 ")");
}

StoredAsset::~StoredAsset()
{
    if (workerThread.joinable())
    {
        workerThread.request_stop();
    }

    // erase from getCreatedAssets entries whose hash matches expectedSha256
    auto &createdAssets = getCreatedAssets();
    if (!createdAssets.empty())
    {
        std::erase_if(createdAssets, [this](const std::string &entry)
                      { return entry.find(expectedSha256) != std::string::npos; });
    }
}

void StoredAsset::verifyOrDownload()
{
    if (state != AssetState::Idle && state != AssetState::Error)
    {
        return;
    }

    workerThread = std::jthread(
        [this](const std::stop_token &stoken)
        {
            state = AssetState::Verifying;
            progressPercent = 0;

            if (fs::exists(localHashPath))
            {
                std::string actualHash = readSha256(localHashPath);
                if (actualHash == expectedSha256)
                {
                    progressPercent = 100;
                    state = AssetState::Complete;
                    logger->info("Asset is valid and up to date: {}", localExtractPath.string());
                    quickLoaded = true;
                    return;
                }
            }

            if (stoken.stop_requested())
            {
                return;
            }

            fs::create_directories(localExtractPath.parent_path());
            ArchiveData archive = performDownload(stoken);

            if (state == AssetState::Extracting)
            {
                if (stoken.stop_requested())
                {
                    cleanup();
                    return;
                }

                deleteOldFiles(localExtractPath);
                extractArchive(archive, localExtractPath);
                cleanup();

                if (state != AssetState::Error)
                {
                    state = AssetState::Complete;

                    // Write hash file
                    std::ofstream hashFile(localHashPath, std::ios::trunc);
                    if (hashFile)
                    {
                        hashFile << expectedSha256;
                    }
                    else
                    {
                        setError("Failed to write hash file for asset.");
                    }
                }
            }
            else
            {
                cleanup();
            }
        });
}

std::string StoredAsset::getError()
{
    std::scoped_lock lock(errorMutex);
    return errorMessage;
}

std::vector<std::string> &StoredAsset::getCreatedAssets()
{
    static std::vector<std::string> createdAssets;
    return createdAssets;
}