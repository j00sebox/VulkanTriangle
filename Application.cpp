#include "Application.hpp"

Application::Application(int width, int height)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, width, "Vulkan Triangle Engine", nullptr, nullptr);

    // GLFW can't create a callback with a member function, so we have to make it outside,
    // but we can store an arbitrary pointer inside the window
    // glfwSetWindowUserPointer(m_window, this);
    // glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);

    m_engine = new Engine(width, height, m_window);
}

Application::~Application()
{
    delete m_engine;

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
        m_engine->render();
    }
}