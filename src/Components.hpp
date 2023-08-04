#pragma once
#include "GPUResources.hpp"
#include <glm/mat4x4.hpp>

struct Mesh
{
    u32 vertex_buffer;
    u32 index_buffer;
    u32 index_count = 0;
};

struct Material
{
    u32 descriptor_set;
    u32 textures[4];
    u32 sampler;
};

struct Model
{
    Mesh mesh;
    Material material;
    glm::mat4 transform{1.f};
};