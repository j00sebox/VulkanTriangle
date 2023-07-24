#pragma once

#include <array>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

struct Vertex
{
    glm::vec2 position;
    glm::vec3 colour;

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
    static std::array<vk::VertexInputAttributeDescription, 2> get_attribute_description()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attribute_descriptions{};

        // position attribute
        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = vk::Format::eR32G32Sfloat;
        attribute_descriptions[0].offset = offsetof(Vertex, position);

        // colour attribute
        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attribute_descriptions[1].offset = offsetof(Vertex, colour);

        return attribute_descriptions;
    }
};
