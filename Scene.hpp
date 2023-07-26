#include "config.hpp"

struct Model;

class Scene
{
public:
    Scene();

    void update();

private:
    std::vector<Model> m_models;
};
