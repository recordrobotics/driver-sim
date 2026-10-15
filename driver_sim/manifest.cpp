#include <optional>

#include "manifest.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <archive.h>
#include <archive_entry.h>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

#include <filesystem>

#include "settings/settingsstore.h"
#include <blackboard_app/logger.h>

#include <packaged.zip.h>

#include "fetch/packagedstoredasset.h"
#include "fetch/remotestoredasset.h"

using namespace blackboard::logger;

namespace fs = std::filesystem;

namespace YAML
{
    template <> struct convert<Manifest::ManifestMetadata>
    {
        static Node encode(const Manifest::ManifestMetadata &rhs)
        {
            Node node;
            node["version"] = rhs.version;
            node["source"] = rhs.source;
            node["shareUrl"] = rhs.shareUrl;
            node["driverSimRepoUrl"] = rhs.driverSimRepoUrl;
            return node;
        }

        static bool decode(const Node &node, Manifest::ManifestMetadata &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.version = node["version"].as<std::string>();
            rhs.source = node["source"].as<std::string>();
            rhs.shareUrl = node["shareUrl"].as<std::string>();
            rhs.driverSimRepoUrl = node["driverSimRepoUrl"].as<std::string>();

            return true;
        }
    };

    template <> struct convert<Manifest::Game>
    {
        static Node encode(const Manifest::Game &rhs)
        {
            Node node;
            node["year"] = rhs.year;
            node["tbaYear"] = rhs.tbaYear;
            node["showFmsUI"] = rhs.showFmsUI;
            return node;
        }

        static bool decode(const Node &node, Manifest::Game &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.year = node["year"].as<std::string>();
            rhs.tbaYear = node["tbaYear"].as<std::string>();
            rhs.showFmsUI = node["showFmsUI"].as<bool>();

            return true;
        }
    };

    template <> struct convert<Manifest::Asset>
    {
        static Node encode(const Manifest::Asset &rhs)
        {
            Node node;
            node["platform"] = rhs.platform;
            if (rhs.url.has_value())
            {
                node["url"] = rhs.url.value();
            }
            node["hash"] = rhs.hash;
            return node;
        }

        static bool decode(const Node &node, Manifest::Asset &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.platform = node["platform"].as<std::string>();
            if (node["url"].IsDefined())
            {
                rhs.url = node["url"].as<std::string>();
            }
            rhs.hash = node["hash"].as<std::string>();

            return true;
        }
    };

    template <> struct convert<Manifest::AssetList>
    {
        static Node encode(const Manifest::AssetList &assets)
        {
            Node node(NodeType::Sequence);

            for (const auto &asset : assets)
            {
                node.push_back(asset);
            }

            return node;
        }

        static bool decode(const Node &node, Manifest::AssetList &assets)
        {
            if (!node.IsSequence())
            {
                return false;
            }

            assets.clear();

            for (const auto &item : node)
            {
                assets.push_back(item.as<Manifest::Asset>());
            }

            return true;
        }
    };

    template <> struct convert<Manifest::Code>
    {
        static Node encode(const Manifest::Code &rhs)
        {
            Node node;
            node["version"] = rhs.version;
            node["commit"] = rhs.commit;
            node["repoUrl"] = rhs.repoUrl;
            node["jarPath"] = rhs.jarPath;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::Code &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.version = node["version"].as<std::string>();
            rhs.commit = node["commit"].as<std::string>();
            rhs.repoUrl = node["repoUrl"].as<std::string>();
            rhs.jarPath = node["jarPath"].as<std::string>();
            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest::JNI>
    {
        static Node encode(const Manifest::JNI &rhs)
        {
            Node node;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::JNI &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest::Robot>
    {
        static Node encode(const Manifest::Robot &rhs)
        {
            Node node;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::Robot &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest::Field>
    {
        static Node encode(const Manifest::Field &rhs)
        {
            Node node;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::Field &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest::JDK>
    {
        static Node encode(const Manifest::JDK &rhs)
        {
            Node node;
            node["version"] = rhs.version;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::JDK &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.version = node["version"].as<std::string>();
            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest::Elastic>
    {
        static Node encode(const Manifest::Elastic &rhs)
        {
            Node node;
            node["assets"] = rhs.assets;
            return node;
        }

        static bool decode(const Node &node, Manifest::Elastic &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.assets = node["assets"].as<Manifest::AssetList>();

            return true;
        }
    };

    template <> struct convert<Manifest>
    {
        static YAML::Node encodeMap(const std::unordered_map<std::string, std::string> &map)
        {
            YAML::Node root(YAML::NodeType::Map);

            for (const auto &[key, value] : map)
            {
                YAML::Node node = root;
                std::size_t start = 0;

                while (start < key.size())
                {
                    const auto dot = key.find('.', start);

                    const std::string part = key.substr(
                        start, dot == std::string::npos ? std::string::npos : dot - start);

                    if (part.empty())
                        throw std::invalid_argument("Invalid map key: " + key);

                    if (dot == std::string::npos)
                    {
                        node[part] = value;
                        break;
                    }

                    node = node[part];
                    start = dot + 1;
                }
            }

            return root;
        }

        static void decodeMapImpl(const YAML::Node &node, const std::string &prefix,
                                  std::unordered_map<std::string, std::string> &output)
        {
            if (node.IsMap())
            {
                for (const auto &entry : node)
                {
                    const std::string key = entry.first.as<std::string>();

                    const std::string fullKey = prefix.empty() ? key : prefix + "." + key;

                    decodeMapImpl(entry.second, fullKey, output);
                }
            }
            else if (node.IsScalar())
            {
                output[prefix] = node.as<std::string>();
            }
            else
            {
                throw std::invalid_argument("Expected a map or scalar at key: " + prefix);
            }
        }

        static std::unordered_map<std::string, std::string> decodeMap(const YAML::Node &root)
        {
            std::unordered_map<std::string, std::string> output;

            decodeMapImpl(root, "", output);

            return output;
        }

        static Node encode(const Manifest &rhs)
        {
            Node node;
            node["manifest"] = rhs.manifest;
            node["game"] = rhs.game;
            node["code"] = rhs.code;
            node["jni"] = rhs.jni;
            node["robot"] = rhs.robot;
            node["field"] = rhs.field;
            node["jdk"] = rhs.jdk;
            node["elastic"] = rhs.elastic;
            node["networktables"] = encodeMap(rhs.networktables);

            return node;
        }

        static bool decode(const Node &node, Manifest &rhs)
        {
            if (!node.IsMap())
            {
                return false;
            }

            rhs.manifest = node["manifest"].as<Manifest::ManifestMetadata>();
            rhs.game = node["game"].as<Manifest::Game>();
            rhs.code = node["code"].as<Manifest::Code>();
            rhs.jni = node["jni"].as<Manifest::JNI>();
            rhs.robot = node["robot"].as<Manifest::Robot>();
            rhs.field = node["field"].as<Manifest::Field>();
            rhs.jdk = node["jdk"].as<Manifest::JDK>();
            rhs.elastic = node["elastic"].as<Manifest::Elastic>();
            rhs.networktables = decodeMap(node["networktables"]);

            return true;
        }
    };
} // namespace YAML

static std::optional<Manifest> packagedManifest;

Manifest Manifest::fromYaml(const std::string &yamlString)
{
    logger->info("Loading manifest from YAML string");
    return YAML::Load(yamlString).as<Manifest>();
}

static fs::path normalizeArchivePath(const char *entryName)
{
    if (!entryName || *entryName == '\0')
        return {};

    std::string name(entryName);
    std::replace(name.begin(), name.end(), '\\', '/');

    fs::path path = fs::path(name).lexically_normal();

    // Archive paths must be relative.
    if (path.is_absolute() || path.has_root_name())
        return {};

    // Reject traversal components.
    for (const auto &component : path)
    {
        if (component == "..")
            return {};
    }

    return path;
}

Manifest Manifest::fromArchive(std::span<const uint8_t> data)
{
    logger->info("Loading manifest from archive file");

    // Decompress the archive file
    struct archive *reader = archive_read_new();

    archive_read_support_filter_all(reader);
    archive_read_support_format_all(reader);

    if (archive_read_open_memory(reader, data.data(), data.size()) != ARCHIVE_OK)
    {
        const char *errorMsg = archive_error_string(reader);
        logger->error("Error loading manifest: Failed to initialize archive reader {}",
                      errorMsg != nullptr ? errorMsg : "unknown error");

        archive_read_free(reader);
        throw std::runtime_error("Failed to initialize archive reader " +
                                 std::string(errorMsg != nullptr ? errorMsg : "unknown error"));
    }

    std::string manifestString;
    std::unordered_set<std::string> packagedAssetDirs;

    archive_entry *entry = nullptr;
    int result = ARCHIVE_OK;
    bool manifestFound = false;

    while ((result = archive_read_next_header(reader, &entry)) == ARCHIVE_OK)
    {
        const fs::path entryPath = normalizeArchivePath(archive_entry_pathname(entry));

        if (entryPath.empty() || entryPath == ".")
        {
            archive_read_data_skip(reader);
            continue;
        }

        const std::string filename = entryPath.generic_string();

        // Find and read manifest.yaml
        if (filename == "manifest.yaml" && archive_entry_filetype(entry) == AE_IFREG)
        {
            std::string contents;
            char buffer[8192];
            la_ssize_t bytesRead = 0;

            while ((bytesRead = archive_read_data(reader, buffer, sizeof(buffer))) > 0)
            {
                contents.append(buffer, static_cast<std::size_t>(bytesRead));
            }

            if (bytesRead < 0)
            {
                const char *errorMsg = archive_error_string(reader);
                const std::string error = errorMsg ? errorMsg : "unknown error";

                logger->error("Error loading manifest: Failed to extract manifest: {}", error);

                archive_read_free(reader);
                throw std::runtime_error("Failed to extract manifest: " + error);
            }

            manifestString = std::move(contents);
            manifestFound = true;
            continue;
        }

        const std::string normalizedName = entryPath.lexically_normal().generic_string();

        if (!normalizedName.empty() && normalizedName != ".")
        {
            const auto firstSlash = normalizedName.find('/');

            if (firstSlash != std::string::npos && firstSlash > 0)
            {
                const std::string assetName = normalizedName.substr(0, firstSlash);

                // Only collect direct children of the archive root
                if (assetName != "." && assetName != "..")
                {
                    if (packagedAssetDirs.insert(assetName).second)
                    {
                        logger->info("Found packaged asset '{}' in manifest", assetName);
                    }
                }
            }
        }

        // Advance past the current entry's contents
        archive_read_data_skip(reader);
    }

    if (result != ARCHIVE_EOF)
    {
        const char *errorMsg = archive_error_string(reader);
        const std::string error = errorMsg ? errorMsg : "unknown error";

        logger->error("Error loading manifest: Failed to read archive: {}", error);

        archive_read_free(reader);
        throw std::runtime_error("Failed to read archive: " + error);
    }

    if (!manifestFound)
    {
        logger->error("Error loading manifest: Manifest file not found in archive");

        archive_read_free(reader);
        throw std::runtime_error("Manifest file not found in archive");
    }

    archive_read_free(reader);

    Manifest manifest = YAML::Load(manifestString).as<Manifest>();
    manifest.packagedAssetDirs = packagedAssetDirs;
    manifest.archiveData = std::vector<uint8_t>(data.begin(), data.end());
    manifest.isArchive = true;

    logger->info("Loaded manifest with {} packaged assets", packagedAssetDirs.size());

    return manifest;
}

Manifest &Manifest::getPackaged()
{
    if (!packagedManifest.has_value())
    {
        logger->info("Loading packaged manifest");
        packagedManifest = Manifest::fromArchive(
            std::span<const uint8_t>(packaged_zip_bytes, sizeof(packaged_zip_bytes)));
    }

    return packagedManifest.value();
}

Manifest &Manifest::getCurrent()
{
    static std::optional<Manifest> currentManifest;

    if (!currentManifest.has_value())
    {
        std::filesystem::path manifestPath = settings::current.manifestPath;
        if (!manifestPath.empty())
        {
            logger->info("Loading manifest from path: {}", manifestPath.string());
            if (!std::filesystem::exists(manifestPath))
            {
                logger->error("Manifest file does not exist at path: {}", manifestPath.string());
                currentManifest = Manifest::getPackaged();
                return currentManifest.value();
            }

            std::ifstream manifestFile(manifestPath, std::ios::binary);
            if (!manifestFile.is_open())
            {
                logger->error("Failed to open manifest file at path: {}", manifestPath.string());
                currentManifest = Manifest::getPackaged();
                return currentManifest.value();
            }

            if (manifestPath.extension() == ".zip")
            {
                logger->info("Loading manifest from ZIP file: {}", manifestPath.string());
                std::vector<uint8_t> manifestData((std::istreambuf_iterator<char>(manifestFile)),
                                                  std::istreambuf_iterator<char>());
                currentManifest = Manifest::fromArchive(manifestData);
            }
            else if (manifestPath.extension() == ".yaml" || manifestPath.extension() == ".yml")
            {
                logger->info("Loading manifest from YAML file: {}", manifestPath.string());
                std::string manifestString((std::istreambuf_iterator<char>(manifestFile)),
                                           std::istreambuf_iterator<char>());
                currentManifest = Manifest::fromYaml(manifestString);
            }
            else
            {
                logger->error("Unsupported manifest file format: {}",
                              manifestPath.extension().string());
                currentManifest = Manifest::getPackaged();
            }
        }
        else
        {
            currentManifest = Manifest::getPackaged();
        }
    }

    return currentManifest.value();
}

std::string Manifest::getNTTopic(const std::string &key)
{
    if (networktables.contains(key))
    {
        return networktables[key];
    }
    else if (getPackaged().networktables.contains(key))
    {
        return getPackaged().networktables[key];
    }
    else
    {
        return "";
    }
}

std::unique_ptr<StoredAsset> Manifest::Asset::getStoredAsset(const std::string &relativePath) const
{
    static std::string prefPath = SDL_GetPrefPath(nullptr, "DriverSim");
    if (!url.has_value())
    {
        // search for packaged asset
        if (Manifest::getCurrent().packagedAssetDirs.contains(relativePath) &&
            Manifest::getCurrent().isArchive)
        {
            return std::make_unique<PackagedStoredAsset>(relativePath, hash, prefPath,
                                                         Manifest::getCurrent().archiveData);
        }
        else if (Manifest::getPackaged().packagedAssetDirs.contains(relativePath) &&
                 Manifest::getPackaged().isArchive)
        {
            return std::make_unique<PackagedStoredAsset>(relativePath, hash, prefPath,
                                                         Manifest::getPackaged().archiveData);
        }
        else
        {
            return {};
        }
    }
    return std::make_unique<RemoteStoredAsset>(relativePath, hash, prefPath, url.value());
}

bool Manifest::Asset::supportsPlatform(std::string_view platform) const
{
    if (this->platform == "all")
    {
        return true;
    }

    std::istringstream stream(this->platform);
    std::string token;
    while (std::getline(stream, token, ','))
    {
        if (string_trim(token) == platform)
        {
            return true;
        }
    }
    return false;
}