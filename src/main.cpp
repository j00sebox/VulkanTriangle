#include "config.hpp"
#include "Application.hpp"

std::vector<std::string> read_file_to_vector(const std::string& filename)
{
    std::ifstream source;
    source.open(filename);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(source, line))
    {
        lines.push_back(line);
    }
    return lines;
}

int main(int argc, char** argv)
{
    std::string scene_name = (argc > 1 ) ? argv[1] : "../scene.txt";
    std::vector<std::string> scene = read_file_to_vector(scene_name);

    Application app(1300, 1000);
    app.load_scene(scene);

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