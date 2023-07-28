#pragma once
#include "GPUResources.hpp"
#include <glm/mat4x4.hpp>

struct Mesh
{
    u32 vertex_buffer;
    u32 index_buffer;
    u32 index_count = 0;
};

struct Model
{
    Mesh mesh;
    glm::mat4 transform{1.f};
};