#pragma once
#include "GPUResources.hpp"
#include <glm/mat4x4.hpp>

struct Mesh
{
    u32             vertex_buffer;
    u32             index_buffer;
    u32             index_count = 0;
};

struct Material
{
    u32 descriptor_set;
    u32 textures[4];
    u32 sampler;
};

struct Model
{
    std::vector<Mesh>           meshes;
    std::vector<Material>       materials;

    glm::mat4       transform{1.f};
};