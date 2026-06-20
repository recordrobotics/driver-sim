#pragma once

#include <blackboard_app/window.h>

namespace field
{
    void init(const blackboard::app::Window &window);
    void startLoadFieldModel();
    void startLoadRobotModel();
    void startNTClient();
    void render(const blackboard::app::Window &window);
    void cleanup();
}