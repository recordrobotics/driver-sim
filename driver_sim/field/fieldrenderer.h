#pragma once

#include <blackboard_app/window.h>
#include "../discord.h"

namespace field
{
    void init(const blackboard::app::Window &window);
    void startLoadFieldModel();
    void startLoadRobotModel();
    void startNTClient();
    void render(const blackboard::app::Window &window, const std::shared_ptr<Discord> &discord);
    void cleanup();

    void setRestartSimulationCallback(std::function<void()> callback);
}