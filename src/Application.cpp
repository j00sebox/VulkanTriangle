#include "Application.hpp"
#include "Input.hpp"
#include "ModelLoader.hpp"
#include "Timer.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

// TODO: move
void coloured_label(const char* label, ImVec4 colour, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, colour);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colour);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, colour);
    ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
}

Application::Application(int width, int height)
{
    Timer timer;
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

    std::cout << "Startup time: " << timer.stop() << "ms\n";
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
    static float render_time = 0.f;
    while (!glfwWindowShouldClose(m_window) && m_running)
    {
        glfwPollEvents();

        if (Input::is_key_pressed(GLFW_KEY_ESCAPE))
        {
            m_running = false;
            continue;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();

        ImGui::NewFrame();

        // imgui commands
        // ImGui::ShowDemoWindow();
        ImGui::Begin("Settings");
        if (ImGui::CollapsingHeader("Diagnostics"))
        {
            ImGui::Text("Avg. %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            ImGui::Text("Render: %.1fms", render_time);
        }

        if (ImGui::CollapsingHeader("Lighting"))
        {
            bool update_data = false;

            LightingData data = m_engine->get_light_data();

            glm::vec3 direct_light_colour = data.direct_light_colour;
            float colour2[3] = {
                    direct_light_colour.x,
                    direct_light_colour.y,
                    direct_light_colour.z
            };

            if(ImGui::ColorEdit3("Direct Light Colour", colour2))
            {
                direct_light_colour.x = colour2[0];
                direct_light_colour.y = colour2[1];
                direct_light_colour.z = colour2[2];

                update_data = true;
            }

            float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
            ImVec2 button_size = { line_height + 3.0f, line_height };

            ImGui::Text("\nDirect Light Position: ");
            coloured_label("x", ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f }, button_size);
            ImGui::SameLine();
            update_data |= ImGui::DragFloat("##x", &data.direct_light_position.x);
            coloured_label("y", ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f }, button_size);
            ImGui::SameLine();
            update_data |= ImGui::DragFloat("##y", &data.direct_light_position.y);
            coloured_label("z", ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f }, button_size);
            ImGui::SameLine();
            update_data |= ImGui::DragFloat("##z", &data.direct_light_position.z);

            if(update_data)
            {
                m_engine->configure_lighting({
                    .direct_light_colour = direct_light_colour,
                    .direct_light_position = data.direct_light_position
                });
            }
        }
        ImGui::End();
        ImGui::Render();

        m_scene->update(get_delta_time());
        Timer timer;
        m_engine->render(m_scene);
        render_time = timer.stop();
    }

    m_engine->wait_for_device_idle();
}

void Application::load_scene(const std::vector<std::string>& scene)
{
    ModelParams params{};
    for(int i = 0; i < scene.size(); i += 4)
    {
        params.path = scene[i].c_str();

        std::vector<float> position_values = get_floats_from_string(scene[i + 1]);
        params.transform = glm::translate(glm::mat4(1.f), {position_values[0], position_values[1], position_values[2]});

        std::vector<float> rotation_values = get_floats_from_string(scene[i + 2]);
        params.transform = glm::rotate(params.transform, glm::radians(rotation_values[0]), {rotation_values[1], rotation_values[2], rotation_values[3]});

        std::vector<float> scale_values = get_floats_from_string(scene[i + 3]);
        params.transform = glm::scale(params.transform, {scale_values[0], scale_values[1], scale_values[2]});

        load_model(params);
        params = {};
    }
}

void Application::load_model(ModelParams params)
{
    Timer timer;
    ModelLoader loader(m_engine, params.path);

    Mesh mesh{};
    loader.load_mesh(mesh);

    Material material{};
    loader.load_material(material);

    m_scene->add_model({ .mesh = mesh, .material = material, .transform = params.transform });
    std::cout << "Model loaded in " << timer.stop() << "ms\n";
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

std::vector<float> Application::get_floats_from_string(std::string line)
{
    size_t location;
    std::string token;
    std::vector<float> values;
    while ((location = line.find(" ")) != std::string::npos)
    {
        token = line.substr(0, location);
        values.push_back(stof(token));
        line.erase(0, location + 1);
    }

    values.push_back(stof(line));

    return values;
}