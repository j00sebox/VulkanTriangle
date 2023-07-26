#pragma once
#include "GPUResources.hpp"
#include <glm/mat4x4.hpp>

struct Mesh
{
    Buffer vertex_buffer;
    Buffer index_buffer;
    u32 index_count = 0;
};

struct Model
{
    Mesh mesh;
    glm::mat4 transform{1.f};
};