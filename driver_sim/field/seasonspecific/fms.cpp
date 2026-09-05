#include <blackboard_app/logger.h>

#include "fms.h"

#include "../../settings/settingsstore.h"

using namespace blackboard::logger;

void FMS::onNTCreated(nt::NetworkTableInstance &ntInst)
{
    matchTimeTopic = ntInst.GetDoubleTopic("/AdvantageKit/DriverStation/MatchTime");
    matchTimeSub = matchTimeTopic.Subscribe(-1.0, {.periodic = settings::current.ntPeriodic});

    redScoreTopic = ntInst.GetDoubleTopic(
        "/SmartDashboard/MapleSim/MatchData/Breakdown/Red Alliance/Improved Score");
    redScoreSub = redScoreTopic.Subscribe(0.0, {.periodic = settings::current.ntPeriodic});

    blueScoreTopic = ntInst.GetDoubleTopic(
        "/SmartDashboard/MapleSim/MatchData/Breakdown/Blue Alliance/Improved Score");
    blueScoreSub = blueScoreTopic.Subscribe(0.0, {.periodic = settings::current.ntPeriodic});

    isAutonomousTopic = ntInst.GetBooleanTopic("/AdvantageKit/DriverStation/Autonomous");
    isAutonomousSub =
        isAutonomousTopic.Subscribe(false, {.periodic = settings::current.ntPeriodic});

    isEnabledTopic = ntInst.GetBooleanTopic("/AdvantageKit/DriverStation/Enabled");
    isEnabledSub = isEnabledTopic.Subscribe(false, {.periodic = settings::current.ntPeriodic});

    allianceStationTopic = ntInst.GetIntegerTopic("/AdvantageKit/DriverStation/AllianceStation");
    allianceStationSub =
        allianceStationTopic.Subscribe(1, {.periodic = settings::current.ntPeriodic});
}

int FMS::getAllianceStation() const { return static_cast<int>(allianceStationSub.Get()); }

int FMS::getBlueScore() const { return static_cast<int>(blueScoreSub.Get()); }

int FMS::getRedScore() const { return static_cast<int>(redScoreSub.Get()); }

int FMS::getMatchTime() const { return static_cast<int>(std::ceil(matchTimeSub.Get())); }

int FMS::getDriverScore() const
{
    int allianceStation = getAllianceStation();
    if (allianceStation >= 1 && allianceStation <= 3)
    {
        return getRedScore();
    }
    if (allianceStation >= 4 && allianceStation <= 6)
    {
        return getBlueScore();
    }

    return 0;
}

int FMS::getOpponentScore() const
{
    int allianceStation = getAllianceStation();
    if (allianceStation >= 1 && allianceStation <= 3)
    {
        return getBlueScore();
    }
    if (allianceStation >= 4 && allianceStation <= 6)
    {
        return getRedScore();
    }

    return 0;
}

DriveMode FMS::getDriveMode() const
{
    if (!isEnabledSub.Get())
    {
        return DriveMode::DISABLED;
    }

    if (isAutonomousSub.Get())
    {
        return DriveMode::AUTONOMOUS;
    }

    return DriveMode::TELEOP;
}

uint64_t FMS::getMatchEndTime() const
{
    double matchTime = matchTimeSub.Get();
    if (matchTime < 0)
    {
        return 0;
    }

    auto currentTimeMillis =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count());
    uint64_t matchEndTimeMillis = currentTimeMillis + static_cast<uint64_t>(matchTime * 1000.0);
    return matchEndTimeMillis;
}