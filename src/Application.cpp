#include "Application.hpp"
#include "Input.hpp"
#include "ModelLoader.hpp"

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

        for(u32 texture : model.material.textures)
        {
            if(texture != m_engine->get_null_texture_handle())
            {
                m_engine->destroy_texture(texture);
            }
        }
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

void Application::load_model(const char* file_name, const char* texture)
{
    ModelLoader loader(m_engine, file_name);

    Mesh mesh{};
    loader.load_mesh(mesh);

    Material material{};
    ModelLoader::load_texture(m_engine, texture, material);

    // TODO: remove later
    glm::mat4 transform = glm::translate(glm::mat4(1.f), {0.f, 0.f, -2.f});
    transform = glm::rotate(transform, glm::radians(90.f), {1.f, 0.f, 0.f});
    transform = glm::scale(transform, {5.f, 5.f, 5.f});

    m_scene->add_model({ .mesh = mesh, .material = material, .transform = transform });
}

void Application::load_primitive(const char* primitive_name)
{
    Mesh mesh{};
    Material material{};
    ModelLoader::load_primitive(m_engine, str_to_primitive_type(primitive_name), mesh);
    ModelLoader::load_texture(m_engine, "../textures/white_on_white.jpeg", material);
    m_scene->add_model({ .mesh = mesh, .material = material });
}

float Application::get_delta_time()
{
    double time = glfwGetTime() * 1000.0;

    auto delta_time = (float)(time - m_prev_time);

    m_prev_time = (float)time;

    return delta_time;
}