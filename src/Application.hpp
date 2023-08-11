#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Renderer.hpp"
#include "Scene.hpp"

struct ModelParams
{
    const char* path;
    glm::mat4 transform;
};

class Application
{
public:
    Application(int width, int height);
    ~Application();

    void run();
    void load_scene(const std::vector<std::string>& scene);
    void load_model(ModelParams params);
    void load_model(const char* file_name, const char* texture);
    void load_primitive(const char* primitive_name);

private:
    Renderer* m_engine;
    GLFWwindow* m_window;
    Scene* m_scene;
    bool m_running;
    float m_prev_time;

    float get_delta_time();
    std::vector<float> get_floats_from_string(std::string line);
};
