#include "Scene.hpp"
#include "OBJLoader.hpp"

struct Model
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;
};

Scene::Scene()
{
    OBJLoader loader("models/viking_room.obj");

    Model model{};
    model.vertices = loader.get_vertices();
    model.indices = loader.get_indices();

    m_models.push_back(model);
}

void Scene::update()
{

}