#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer.hpp"
#include "Scene.hpp"

class Application
{
public:
    Application(int width, int height);
    ~Application();

    void run();
    void load_model(const char* file_name);

    // FIXME
    Scene* m_scene;

private:
    Renderer* m_engine;
    GLFWwindow* m_window;

    float m_prev_time;
    float get_delta_time();
};
