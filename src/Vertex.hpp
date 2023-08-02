#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normals;
    glm::vec2 uv;

    static vk::VertexInputBindingDescription get_binding_description()
    {
        vk::VertexInputBindingDescription binding_desc{};
        binding_desc.binding = 0; // index of binding in array
        binding_desc.stride = sizeof(Vertex); // number of bytes between entries
        binding_desc.inputRate = vk::VertexInputRate::eVertex; // move to the next entry after each vertex

        return binding_desc;
    }

    // describes how extract vertex attribute from vertex data chunk
    // need 1 struct per vertex attribute
    static std::array<vk::VertexInputAttributeDescription, 3> get_attribute_description()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attribute_descriptions{};

        // position attribute
        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[0].offset = offsetof(Vertex, position);

        // vertex normal defined by model
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(Vertex, normals);

        // texture coordinates
        attribute_descriptions[2].binding = 0;
        attribute_descriptions[2].location = 2;
        attribute_descriptions[2].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[2].offset = offsetof(Vertex, uv);

        return attribute_descriptions;
    }
};
