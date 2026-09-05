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

#include "../fms.h"
#include "../ifmsui.h"

class Rebuilt2026FMSUI : public IFMSUI
{
  public:
    explicit Rebuilt2026FMSUI(std::shared_ptr<FMS> fms);
    ~Rebuilt2026FMSUI() override;

    Rebuilt2026FMSUI(const Rebuilt2026FMSUI &) = delete;
    Rebuilt2026FMSUI &operator=(const Rebuilt2026FMSUI &) = delete;
    Rebuilt2026FMSUI(Rebuilt2026FMSUI &&) noexcept = default;
    Rebuilt2026FMSUI &operator=(Rebuilt2026FMSUI &&) noexcept = default;

    void onNTCreated(nt::NetworkTableInstance &ntInst) override;

    void render(ImVec2 winSize) override;
    void tagFieldObjects(std::unordered_map<std::string, std::string> &tags) override;
    void postProcessField(std::vector<Mesh> &fieldMeshes) override;

  private:
    void drawFMSUI(ImVec2 winSize);
    void updateHubMaterials();

    std::shared_ptr<FMS> fms;

    nt::BooleanTopic redHubActiveTopic;
    nt::BooleanSubscriber redHubActiveSub;

    nt::BooleanTopic blueHubActiveTopic;
    nt::BooleanSubscriber blueHubActiveSub;

    nt::BooleanTopic redHubLedTopic;
    nt::BooleanSubscriber redHubLedSub;

    nt::BooleanTopic blueHubLedTopic;
    nt::BooleanSubscriber blueHubLedSub;

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