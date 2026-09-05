#include "manifest.h"

Manifest &Manifest::getPackaged()
{
    static Manifest packagedManifest(
        "1.0.0", "Packaged", "2026.7.16+", "d1d8cba",
        "https://github.com/recordrobotics/driver-sim",
        "https://github.com/recordrobotics/2026-robot",
        "https://github.com/recordrobotics/2026-robot/releases/latest",
        // game year | tba (logo) year
        "2026", "2026",
        // jdk
        "17.0.16+8",
        "https://api.adoptium.net/v3/binary/version/jdk-17.0.16%2B8/windows/x64/jdk/hotspot/normal/"
        "eclipse?project=jdk",
        "8c7cfff78a55c56ebaf470ed6a89c6466b47d8274bdabdda997d7507c20325c5",
        // elastic
        "https://github.com/Gold872/elastic_dashboard/releases/download/v2026.1.2/"
        "Elastic-Windows_portable.zip",
        "6581e66eb237f9d615afb94077d89a03e2cdd7ce2d57f11c8cc5153821493ad7",
        // field
        "https://github.com/Mechanical-Advantage/AdvantageScopeAssets/releases/download/archive-v1/"
        "Field3d_2026FRCFieldV1.zip",
        "0f2abde864422367dd1bc3254da23b36a3d82eb727d5dac0a0f2231bdc397e31",
        // robot
        "https://hamster1.ddns.net/"
        "robot-b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462.zip",
        "b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462",
        // jni
        "https://hamster1.ddns.net/"
        "jni-0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5.zip",
        "0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5",
        // robot code
        "", "7998021ca2a0f0d8867173cd7fcf8f4b15fb36d011d98df55b00bebb76732878", "2026-robot.jar",
        // show FMS UI
        true);
    return packagedManifest;
}

Manifest &Manifest::getCurrent()
{
    static Manifest currentManifest = Manifest::getPackaged();
    return currentManifest;
}