#pragma once

#include <networktables/BooleanTopic.h>
#include <networktables/DoubleTopic.h>
#include <networktables/IntegerTopic.h>
#include <networktables/NetworkTable.h>
#include <networktables/NetworkTableInstance.h>

#include <blackboard_app/gui.h>

#include "../teamassigner.h"
#include "../teamlogocache.h"

#include "../../mesh.h"
#include <vector>

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
    void postProcessField(std::vector<Mesh> &fieldMeshes);

    [[nodiscard]] int getDriverScore() const;
    [[nodiscard]] int getOpponentScore() const;

    [[nodiscard]] std::string getDriveMode() const;

    [[nodiscard]] uint64_t getMatchEndTime() const;

  private:
    void drawFMSUI(ImVec2 winSize);
    void updateHubMaterials();

    nt::DoubleTopic matchTimeTopic;
    nt::DoubleSubscriber matchTimeSub;

    nt::BooleanTopic redHubActiveTopic;
    nt::BooleanSubscriber redHubActiveSub;

    nt::BooleanTopic blueHubActiveTopic;
    nt::BooleanSubscriber blueHubActiveSub;

    nt::BooleanTopic redHubLedTopic;
    nt::BooleanSubscriber redHubLedSub;

    nt::BooleanTopic blueHubLedTopic;
    nt::BooleanSubscriber blueHubLedSub;

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

    TeamAssigner teamAssigner;
    TeamLogoCache logoCache;

    ImFont *font;
    blackboard::gui::ImTexture firstAgeBanner;
    blackboard::gui::ImTexture rebuiltBanner;
    blackboard::gui::ImTexture arrowIcon;
    blackboard::gui::ImTexture fuelBlueIcon;
    blackboard::gui::ImTexture fuelRedIcon;

    Material *hubRedLightMaterial = nullptr;
    Material *hubBlueLightMaterial = nullptr;
};