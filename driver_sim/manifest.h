#pragma once

#include <string>

class Manifest
{
  public:
    std::string &getManifestVersion() { return manifestVersion; }
    std::string &getSourceType() { return sourceType; }
    std::string &getBuildVersion() { return buildVersion; }
    std::string &getCommitHash() { return commitHash; }

    std::string &getDriverSimRepoUrl() { return driverSimRepoUrl; }
    std::string &getRobotCodeRepoUrl() { return robotCodeRepoUrl; }
    std::string &getRobotDownloadUrl() { return robotDownloadUrl; }

    std::string &getGameYear() { return gameYear; }
    std::string &getTbaYear() { return tbaYear; }

    std::string &getJdkVersion() { return jdkVersion; }
    std::string &getJdkDownloadUrl() { return jdkDownloadUrl; }
    std::string &getJdkHash() { return jdkHash; }

    std::string &getElasticDownloadUrl() { return elasticDownloadUrl; }
    std::string &getElasticHash() { return elasticHash; }

    std::string &getFieldDownloadUrl() { return fieldDownloadUrl; }
    std::string &getFieldHash() { return fieldHash; }

    std::string &getRobotAssetDownloadUrl() { return robotAssetDownloadUrl; }
    std::string &getRobotAssetHash() { return robotAssetHash; }

    std::string &getJniDownloadUrl() { return jniDownloadUrl; }
    std::string &getJniHash() { return jniHash; }

    std::string &getRobotCodeDownloadUrl() { return robotCodeDownloadUrl; }
    std::string &getRobotCodeHash() { return robotCodeHash; }
    std::string &getRobotCodeJarName() { return robotCodeJarName; }

    static Manifest &getCurrent();
    static Manifest &getPackaged();

  private:
    Manifest(std::string manifestVersion, std::string sourceType, std::string buildVersion,
             std::string commitHash, std::string driverSimRepoUrl, std::string robotCodeRepoUrl,
             std::string robotDownloadUrl, std::string gameYear, std::string tbaYear,
             std::string jdkVersion, std::string jdkDownloadUrl, std::string jdkHash,
             std::string elasticDownloadUrl, std::string elasticHash, std::string fieldDownloadUrl,
             std::string fieldHash, std::string robotAssetDownloadUrl, std::string robotAssetHash,
             std::string jniDownloadUrl, std::string jniHash, std::string robotCodeDownloadUrl,
             std::string robotCodeHash, std::string robotCodeJarName)
        : manifestVersion(std::move(manifestVersion)), sourceType(std::move(sourceType)),
          buildVersion(std::move(buildVersion)), commitHash(std::move(commitHash)),
          driverSimRepoUrl(std::move(driverSimRepoUrl)),
          robotCodeRepoUrl(std::move(robotCodeRepoUrl)),
          robotDownloadUrl(std::move(robotDownloadUrl)), gameYear(std::move(gameYear)),
          tbaYear(std::move(tbaYear)), jdkVersion(std::move(jdkVersion)),
          jdkDownloadUrl(std::move(jdkDownloadUrl)), jdkHash(std::move(jdkHash)),
          elasticDownloadUrl(std::move(elasticDownloadUrl)), elasticHash(std::move(elasticHash)),
          fieldDownloadUrl(std::move(fieldDownloadUrl)), fieldHash(std::move(fieldHash)),
          robotAssetDownloadUrl(std::move(robotAssetDownloadUrl)),
          robotAssetHash(std::move(robotAssetHash)), jniDownloadUrl(std::move(jniDownloadUrl)),
          jniHash(std::move(jniHash)), robotCodeDownloadUrl(std::move(robotCodeDownloadUrl)),
          robotCodeHash(std::move(robotCodeHash)), robotCodeJarName(std::move(robotCodeJarName))
    {
    }

    std::string manifestVersion;
    std::string sourceType;
    std::string buildVersion;
    std::string commitHash;

    std::string driverSimRepoUrl;
    std::string robotCodeRepoUrl;
    std::string robotDownloadUrl;

    std::string gameYear;
    std::string tbaYear;

    std::string jdkVersion;
    std::string jdkDownloadUrl;
    std::string jdkHash;

    std::string elasticDownloadUrl;
    std::string elasticHash;

    std::string fieldDownloadUrl;
    std::string fieldHash;

    std::string robotAssetDownloadUrl;
    std::string robotAssetHash;

    std::string jniDownloadUrl;
    std::string jniHash;

    std::string robotCodeDownloadUrl;
    std::string robotCodeHash;
    std::string robotCodeJarName;
};