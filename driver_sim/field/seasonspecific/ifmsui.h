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
    virtual void preProcessField(std::unordered_map<std::string, std::string> &tags,
                                 std::unordered_map<std::string, MeshLOD> &lodOverrides) = 0;
    virtual void postProcessField(std::vector<Mesh> &fieldMeshes) = 0;
};