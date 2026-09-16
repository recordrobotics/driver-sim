#include <optional>

#include "manifest.h"
#include "utils.h"

#include <SDL3/SDL.h>
#include <miniz.h>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

#include <packaged.zip.h>

#include "fetch/packagedstoredasset.h"
#include "fetch/remotestoredasset.h"

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
    return YAML::Load(yamlString).as<Manifest>();
}

Manifest Manifest::fromZip(std::span<const uint8_t> data)
{
    // Decompress the zip file
    std::shared_ptr<mz_zip_archive> zip = std::make_shared<mz_zip_archive>();
    mz_zip_zero_struct(zip.get());
    if (!mz_zip_reader_init_mem(zip.get(), data.data(), data.size(), 0))
    {
        throw std::runtime_error("Failed to initialize zip reader");
    }

    // Find the manifest file
    int manifestIndex = mz_zip_reader_locate_file(zip.get(), "manifest.yaml", nullptr, 0);
    if (manifestIndex < 0)
    {
        mz_zip_reader_end(zip.get());
        throw std::runtime_error("Manifest file not found in zip");
    }

    // Read the manifest file
    size_t manifestSize = 0;
    void *manifestData = mz_zip_reader_extract_to_heap(zip.get(), manifestIndex, &manifestSize, 0);
    if (manifestData == nullptr)
    {
        mz_zip_reader_end(zip.get());
        throw std::runtime_error("Failed to extract manifest from zip");
    }

    // Parse the manifest
    std::string manifestString(static_cast<char *>(manifestData), manifestSize);
    free(manifestData);

    // Get list of directory names in the root of the zip file - these are packaged assets
    std::unordered_set<std::string> packagedAssetDirs;
    mz_uint numFiles = mz_zip_reader_get_num_files(zip.get());
    for (mz_uint i = 0; i < numFiles; ++i)
    {
        mz_zip_archive_file_stat fileStat;
        if (!mz_zip_reader_file_stat(zip.get(), i, &fileStat))
        {
            mz_zip_reader_end(zip.get());
            throw std::runtime_error("Failed to get file stat from zip");
        }

        std::string filename(fileStat.m_filename);
        if (mz_zip_reader_is_file_a_directory(zip.get(), i) != 0 &&
            filename.find('/') == filename.size() - 1)
        {
            packagedAssetDirs.insert(filename.substr(0, filename.size() - 1));
        }
    }

    Manifest manifest = YAML::Load(manifestString).as<Manifest>();
    manifest.packagedAssetDirs = packagedAssetDirs;
    manifest.zip = zip;
    manifest.isZip = true;
    return manifest;
}

Manifest &Manifest::getPackaged()
{
    if (!packagedManifest.has_value())
    {
        packagedManifest = Manifest::fromZip(
            std::span<const uint8_t>(packaged_zip_bytes, sizeof(packaged_zip_bytes)));
    }

    return packagedManifest.value();
}

Manifest &Manifest::getCurrent()
{
    static Manifest currentManifest = Manifest::getPackaged();
    return currentManifest;
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

void Manifest::close()
{
    if (isZip)
    {
        mz_zip_reader_end(zip.get());
        zip.reset();
        isZip = false;
    }
}

std::unique_ptr<StoredAsset> Manifest::Asset::getStoredAsset(const std::string &relativePath) const
{
    static std::string prefPath = SDL_GetPrefPath(nullptr, "DriverSim");
    if (!url.has_value())
    {
        // search for packaged asset
        if (Manifest::getCurrent().packagedAssetDirs.contains(relativePath) &&
            Manifest::getCurrent().isZip)
        {
            return std::make_unique<PackagedStoredAsset>(relativePath, hash, prefPath,
                                                         Manifest::getCurrent().zip);
        }
        else if (Manifest::getPackaged().packagedAssetDirs.contains(relativePath) &&
                 Manifest::getPackaged().isZip)
        {
            return std::make_unique<PackagedStoredAsset>(relativePath, hash, prefPath,
                                                         Manifest::getPackaged().zip);
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