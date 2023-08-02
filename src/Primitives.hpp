#pragma once

#include "config.hpp"
#include "Vertex.hpp"

// TODO: Add more
enum class PrimitiveTypes
{
    None = 0,
    Cube,
    Quad
};

std::string primitive_type_to_str(PrimitiveTypes pt);
PrimitiveTypes str_to_primitive_type(const char* name);

struct Cube
{
    static const std::vector<Vertex> vertices;
    static const std::vector<u32> indices;
};

struct Quad
{
    static const std::vector<Vertex> vertices;
    static const std::vector<u32> indices;
};

