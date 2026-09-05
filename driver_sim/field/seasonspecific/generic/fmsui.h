#pragma once

#include <blackboard_app/gui.h>

#include "../teamassigner.h"
#include "../teamlogocache.h"

#include "../../mesh.h"
#include <vector>

#include "../fms.h"
#include "../ifmsui.h"

class GenericFMSUI : public IFMSUI
{
  public:
    explicit GenericFMSUI(std::shared_ptr<FMS> fms);
    ~GenericFMSUI() override = default;

    GenericFMSUI(const GenericFMSUI &) = delete;
    GenericFMSUI &operator=(const GenericFMSUI &) = delete;
    GenericFMSUI(GenericFMSUI &&) noexcept = default;
    GenericFMSUI &operator=(GenericFMSUI &&) noexcept = default;

    void onNTCreated(nt::NetworkTableInstance &ntInst) override {}

    void render(ImVec2 winSize) override;
    void tagFieldObjects(std::unordered_map<std::string, std::string> &tags) override {}
    void postProcessField(std::vector<Mesh> &fieldMeshes) override {}

  private:
    void drawFMSUI(ImVec2 winSize);

    std::shared_ptr<FMS> fms;

    TeamAssigner teamAssigner;
    TeamLogoCache logoCache;

    ImFont *font;
};