#include "config.hpp"
#include "Application.hpp"

int main() 
{
    // HelloTriangle app;
    Application app(800, 600);
    app.load_model("../models/viking_room.obj");

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