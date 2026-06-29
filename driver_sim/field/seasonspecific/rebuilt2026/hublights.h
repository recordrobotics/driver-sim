#pragma once

#include <unordered_map>
#include <array>
#include <string>
#include "../../mesh.h"
#include "../../../utils.h"

namespace Rebuilt2026
{
    constexpr std::string_view redHubLedTag = "red_hub_led";
    constexpr std::string_view blueHubLedTag = "blue_hub_led";

    constexpr std::array<float, 4> hubLedOffColor = {0.0f, 0.0f, 0.0f, 0.0f};
    constexpr std::array<float, 4> redHubLedColor = {1.0f, 0.0f, 0.0f, pow2(6.39f)};
    constexpr std::array<float, 4> blueHubLedColor = {0.0f, 0.0f, 1.0f, pow2(6.39f)};

    inline void addHubLedTags(std::unordered_map<std::string, std::string> &tags)
    {
        tags["GE-26309_Hub_Front_Diffuser"] = redHubLedTag;
        tags["GE-26311_Hub_Side_Diffuser"] = redHubLedTag;
        tags["GE-26311_Hub_Side_Diffuser_1"] = redHubLedTag;
        tags["GE-26310_Hub_Rear_Diffuser"] = redHubLedTag;

        tags["GE-26309_Hub_Front_Diffuser_1"] = blueHubLedTag;
        tags["GE-26311_Hub_Side_Diffuser_2"] = blueHubLedTag;
        tags["GE-26311_Hub_Side_Diffuser_3"] = blueHubLedTag;
        tags["GE-26310_Hub_Rear_Diffuser_1"] = blueHubLedTag;
    }

    inline Material *getTaggedMaterial(std::vector<Mesh> &meshes, const std::string_view &tag)
    {
        for (auto &mesh : meshes)
        {
            if (mesh.tag == tag)
            {
                return &mesh.material;
            }
        }
        return nullptr;
    }
};