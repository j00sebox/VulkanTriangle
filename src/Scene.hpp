#pragma once
#include "config.hpp"
#include "Components.hpp"
#include "Camera.hpp"

class Scene
{
public:
    Scene() = default;
    void update(float delta_time);
    void add_model(const Model& model);
    void add_model(Model&& model);

    std::vector<Model> models;

    Camera camera;
};
