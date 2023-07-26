#pragma once
#include "config.hpp"
#include "Components.hpp"

class Scene
{
public:
    Scene() = default;
    void update();
    void add_model(const Model& model);
    void add_model(Model&& model);

    std::vector<Model> models;
};
