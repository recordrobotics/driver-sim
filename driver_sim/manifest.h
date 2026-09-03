#pragma once

#include <string>

class Manifest
{
  public:
    std::string &getManifestVersion();
    std::string &getSourceType();
    std::string &getBuildVersion();
    std::string &getCommitHash();

    std::string &getDriverSimRepoUrl();
    std::string &getRobotCodeRepoUrl();
    std::string &getRobotDownloadUrl();

    static Manifest &getCurrent();
    static Manifest &getPackaged();

  private:
    Manifest(std::string manifestVersion, std::string sourceType, std::string buildVersion,
             std::string commitHash, std::string driverSimRepoUrl, std::string robotCodeRepoUrl,
             std::string robotDownloadUrl)
        : manifestVersion(std::move(manifestVersion)), sourceType(std::move(sourceType)),
          buildVersion(std::move(buildVersion)), commitHash(std::move(commitHash)),
          driverSimRepoUrl(std::move(driverSimRepoUrl)),
          robotCodeRepoUrl(std::move(robotCodeRepoUrl)),
          robotDownloadUrl(std::move(robotDownloadUrl))
    {
    }

    std::string manifestVersion;
    std::string sourceType;
    std::string buildVersion;
    std::string commitHash;

    std::string driverSimRepoUrl;
    std::string robotCodeRepoUrl;
    std::string robotDownloadUrl;
};