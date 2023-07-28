#include "Scene.hpp"

void Scene::update(float delta_time)
{
    camera.update(delta_time);
}

void Scene::add_model(const Model& model)
{
    models.push_back(model);
}

void Scene::add_model(Model&& model)
{
    models.emplace_back(model);
}