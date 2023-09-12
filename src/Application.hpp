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
    void load_scene(const std::vector<std::string>& scene);
    void load_primitive(const char* primitive_name);

private:
    Renderer* m_renderer;
    GLFWwindow* m_window;
    Scene* m_scene;
    enki::TaskScheduler* m_scheduler;
    bool m_running;
    float m_prev_time;

    float get_delta_time();
    static std::vector<float> get_floats_from_string(std::string line);
};
