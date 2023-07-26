#include "Scene.hpp"
#include "OBJLoader.hpp"

void Scene::update()
{

}

void Scene::add_model(const Model& model)
{
    models.push_back(model);
}

void Scene::add_model(Model&& model)
{
    models.emplace_back(model);
}