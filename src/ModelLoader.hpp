#pragma once

#include "config.hpp"
#include "Primitives.hpp"
#include "Components.hpp"
#include "Renderer.hpp"
#include "Vertex.hpp"
#include "ResourceLoader.hpp"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

class ModelLoader
{
public:
    explicit ModelLoader(Renderer* renderer, const char* file_path);
    void load_mesh(Mesh& mesh);
    void load_material(Material& material);
    const char* get_name();

    static void load_primitive(Renderer* renderer, PrimitiveTypes primitive, Mesh& mesh);
    static void load_texture(Renderer* renderer, const char* texture_path, Material& material);

private:
    static std::vector<Vertex> get_vertices(aiMesh* mesh);
    static std::vector<u32> get_indices(aiMesh* mesh);

    Renderer* m_renderer;
    Assimp::Importer m_importer;
    const aiScene* m_scene;
    std::string m_base_dir;
};

