#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Engine.hpp"

class Application
{
public:
    Application(int width, int height);
    ~Application();

    void run();

private:
    Engine* m_engine;
    GLFWwindow* m_window;
};
