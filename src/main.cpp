#include "config.hpp"
#include "Application.hpp"

int main() 
{
    // HelloTriangle app;
    Application app(800, 600);
    app.load_model("../models/viking_room.obj");
    app.load_model("../models/teapot.obj");
    app.m_scene->models.back().transform = glm::scale(glm::mat4(1.f), {0.1f, 0.1f, 0.1f});

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}