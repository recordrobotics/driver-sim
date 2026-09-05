#pragma once

#include <imgui/imgui.h>
#include <vector>

#include "../mesh.h"

class IFMSUI
{
  public:
    virtual ~IFMSUI() = default;

    virtual void onNTCreated(nt::NetworkTableInstance &ntInst) = 0;

    virtual void render(ImVec2 winSize) = 0;
    virtual void tagFieldObjects(std::unordered_map<std::string, std::string> &tags) = 0;
    virtual void postProcessField(std::vector<Mesh> &fieldMeshes) = 0;
};