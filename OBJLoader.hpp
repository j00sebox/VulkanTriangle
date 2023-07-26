#pragma once

#include "config.hpp"
#include "Vertex.hpp"

class OBJLoader
{
public:
    OBJLoader(const char* file_path);

    const std::vector<Vertex>& get_vertices() const { return m_vertices; }
    const std::vector<u32>& get_indices() const { return m_indices; }

private:
    std::vector<Vertex> m_vertices;
    std::vector<u32> m_indices;
};
