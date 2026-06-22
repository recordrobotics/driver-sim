#pragma once

#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>
#include <networktables/DoubleTopic.h>
#include <networktables/BooleanTopic.h>
#include <networktables/IntegerTopic.h>

#include <blackboard_app/gui.h>

#include "../teamassigner.h"
#include "../teamlogocache.h"

using blackboard::gui::string_hex_to_rgba_float;

class Rebuilt2026FMSUI
{
public:
    Rebuilt2026FMSUI(nt::NetworkTableInstance &ntInst);
    ~Rebuilt2026FMSUI();

    Rebuilt2026FMSUI(const Rebuilt2026FMSUI &) = delete;
    Rebuilt2026FMSUI &operator=(const Rebuilt2026FMSUI &) = delete;
    Rebuilt2026FMSUI(Rebuilt2026FMSUI &&) noexcept = default;
    Rebuilt2026FMSUI &operator=(Rebuilt2026FMSUI &&) noexcept = default;

    void render(ImVec2 winSize);

private:
    nt::DoubleTopic matchTimeTopic;
    nt::DoubleSubscriber matchTimeSub;

    nt::BooleanTopic redHubActiveTopic;
    nt::BooleanSubscriber redHubActiveSub;

    nt::BooleanTopic blueHubActiveTopic;
    nt::BooleanSubscriber blueHubActiveSub;

    nt::DoubleTopic redScoreTopic;
    nt::DoubleSubscriber redScoreSub;

    nt::DoubleTopic blueScoreTopic;
    nt::DoubleSubscriber blueScoreSub;

    nt::BooleanTopic isAutonomousTopic;
    nt::BooleanSubscriber isAutonomousSub;

    nt::IntegerTopic allianceStationTopic;
    nt::IntegerSubscriber allianceStationSub;

    TeamAssigner teamAssigner;
    TeamLogoCache logoCache;

    ImFont *font;
    blackboard::gui::ImTexture firstAgeBanner;
    blackboard::gui::ImTexture rebuiltBanner;
    blackboard::gui::ImTexture arrowIcon;
    blackboard::gui::ImTexture fuelBlueIcon;
    blackboard::gui::ImTexture fuelRedIcon;
};