#include "Application.hpp"
#include "Input.hpp"

Application::Application(int width, int height)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(width, height, "Vulkan Triangle Engine", nullptr, nullptr);

    // GLFW can't create a callback with a member function, so we have to make it outside,
    // but we can store an arbitrary pointer inside the window
    // glfwSetWindowUserPointer(m_window, this);
    // glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);

    m_scene = new Scene();
    Input::m_window_handle = m_window;
    m_scene->camera.resize(width, height);
    m_engine = new Renderer(m_window);
}

Application::~Application()
{
    // TODO: remove later
    for(Model model : m_scene->models)
    {
        m_engine->destroy_buffer(model.mesh.vertex_buffer);
        m_engine->destroy_buffer(model.mesh.index_buffer);
        m_engine->destroy_texture(model.material.texture);
        m_engine->destroy_sampler(model.material.sampler);
    }

    delete m_scene;
    delete m_engine;

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void Application::run()
{
    m_running = true;
    while (!glfwWindowShouldClose(m_window) && m_running)
    {
        glfwPollEvents();

        if (Input::is_key_pressed(GLFW_KEY_ESCAPE))
        {
            m_running = false;
            continue;
        }

        m_scene->update(get_delta_time());
        m_engine->render(m_scene);
    }

    m_engine->wait_for_device_idle();
}

void Application::load_model(const char* file_name)
{
    OBJLoader loader(file_name);
    m_scene->add_model(m_engine->load_model(loader));
}

float Application::get_delta_time()
{
    double time = glfwGetTime() * 1000.0;

    auto delta_time = (float)(time - m_prev_time);

    m_prev_time = (float)time;

    return delta_time;
}