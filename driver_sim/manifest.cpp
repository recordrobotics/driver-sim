#include "manifest.h"

std::string &Manifest::getManifestVersion() { return manifestVersion; }

std::string &Manifest::getSourceType() { return sourceType; }

std::string &Manifest::getBuildVersion() { return buildVersion; }

std::string &Manifest::getCommitHash() { return commitHash; }

std::string &Manifest::getDriverSimRepoUrl() { return driverSimRepoUrl; }

std::string &Manifest::getRobotCodeRepoUrl() { return robotCodeRepoUrl; }

std::string &Manifest::getRobotDownloadUrl() { return robotDownloadUrl; }

Manifest &Manifest::getPackaged()
{
    static Manifest packagedManifest(
        "1.0.0", "Packaged", "2026.7.16+", "d1d8cba",
        "https://github.com/recordrobotics/driver-sim",
        "https://github.com/recordrobotics/2026-robot",
        "https://github.com/recordrobotics/2026-robot/releases/latest");
    return packagedManifest;
}

Manifest &Manifest::getCurrent()
{
    static Manifest currentManifest = Manifest::getPackaged();
    return currentManifest;
}