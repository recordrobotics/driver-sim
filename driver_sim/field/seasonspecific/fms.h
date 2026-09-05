#pragma once

#include <cstdint>
#include <networktables/BooleanTopic.h>
#include <networktables/DoubleTopic.h>
#include <networktables/IntegerTopic.h>
#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>

enum class DriveMode : uint8_t
{
    DISABLED = 0,
    TELEOP = 1,
    AUTONOMOUS = 2,
};

class FMS
{
  public:
    FMS() = default;
    ~FMS() = default;

    FMS(const FMS &) = delete;
    FMS &operator=(const FMS &) = delete;
    FMS(FMS &&) noexcept = default;
    FMS &operator=(FMS &&) noexcept = default;

    void onNTCreated(nt::NetworkTableInstance &ntInst);

    [[nodiscard]] int getAllianceStation() const;
    [[nodiscard]] int getBlueScore() const;
    [[nodiscard]] int getRedScore() const;
    [[nodiscard]] int getMatchTime() const;

    [[nodiscard]] int getDriverScore() const;
    [[nodiscard]] int getOpponentScore() const;

    [[nodiscard]] DriveMode getDriveMode() const;

    [[nodiscard]] uint64_t getMatchEndTime() const;

  private:
    nt::DoubleTopic matchTimeTopic;
    nt::DoubleSubscriber matchTimeSub;

    nt::DoubleTopic redScoreTopic;
    nt::DoubleSubscriber redScoreSub;

    nt::DoubleTopic blueScoreTopic;
    nt::DoubleSubscriber blueScoreSub;

    nt::BooleanTopic isAutonomousTopic;
    nt::BooleanSubscriber isAutonomousSub;

    nt::BooleanTopic isEnabledTopic;
    nt::BooleanSubscriber isEnabledSub;

    nt::IntegerTopic allianceStationTopic;
    nt::IntegerSubscriber allianceStationSub;
};