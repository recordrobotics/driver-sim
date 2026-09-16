#pragma once

#include "fetch/storedasset.h"
#include <miniz.h>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

struct Manifest
{
  public:
    struct ManifestMetadata
    {
        std::string version;
        std::string source;
        std::string shareUrl;
        std::string driverSimRepoUrl;
    };

    struct Game
    {
        std::string year;
        std::string tbaYear;
        bool showFmsUI;
    };

    struct Asset
    {
        std::string platform;
        std::optional<std::string> url;
        std::string hash;

        // platform can be comma separated list of platforms or "all"
        [[nodiscard]] bool supportsPlatform(std::string_view platform) const;

        [[nodiscard]] std::unique_ptr<StoredAsset>
        getStoredAsset(const std::string &relativePath) const;
    };

    struct AssetList : public std::vector<Asset>
    {
      public:
        [[nodiscard]] std::optional<Asset> getAssetForPlatform(std::string_view platform) const
        {
            for (const auto &asset : *this)
            {
                if (asset.supportsPlatform(platform))
                {
                    return asset;
                }
            }
            return std::nullopt;
        }

        [[nodiscard]] std::unique_ptr<StoredAsset>
        getStoredAssetForPlatform(const std::string &relativePath, std::string_view platform) const
        {
            auto asset = getAssetForPlatform(platform);
            if (asset.has_value())
            {
                return asset->getStoredAsset(relativePath);
            }
            return {};
        }

        [[nodiscard]] std::unique_ptr<StoredAsset>
        getStoredAssetForCurrentPlatform(const std::string &relativePath) const
        {
            std::string platform;
#ifdef _WIN32
            platform = "windows-x64";
#elif __APPLE__
#if defined(__aarch64__)
            platform = "macos-aarch64";
#else
            platform = "macos-x64";
#endif
#elif __linux__
            platform = "linux-x64";
#endif
            return getStoredAssetForPlatform(relativePath, platform);
        }
    };

    struct Code
    {
        std::string version;
        std::string commit;
        std::string repoUrl;

        std::string jarPath;

        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("code");
        }
    };

    struct JNI
    {
        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("jni");
        }
    };

    struct Robot
    {
        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("robot");
        }
    };

    struct Field
    {
        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("field");
        }
    };

    struct JDK
    {
        std::string version;
        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("jdk");
        }
    };

    struct Elastic
    {
        AssetList assets;

        [[nodiscard]] std::unique_ptr<StoredAsset> getStoredAsset() const
        {
            return assets.getStoredAssetForCurrentPlatform("elastic");
        }
    };

    ManifestMetadata manifest;
    Game game;
    Code code;
    JNI jni;
    Robot robot;
    Field field;
    JDK jdk;
    Elastic elastic;

    std::unordered_map<std::string, std::string> networktables;

    std::unordered_set<std::string> packagedAssetDirs;
    bool isZip;
    std::shared_ptr<mz_zip_archive> zip;

    std::string getNTTopic(const std::string &key);

    void close();

    static Manifest fromYaml(const std::string &yamlString);
    static Manifest fromZip(std::span<const uint8_t> data);

    static Manifest &getCurrent();
    static Manifest &getPackaged();
};