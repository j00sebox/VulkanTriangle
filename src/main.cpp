#include "config.hpp"
#include "Application.hpp"

int main() 
{
    // HelloTriangle app;
    Application app(1300, 1000);
    app.load_model("../models/bunny/scene.gltf", "../textures/white_on_white.jpeg");
    // app.load_primitive("cube");

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