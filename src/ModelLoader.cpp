#include "ModelLoader.hpp"

ModelLoader::ModelLoader(Renderer* renderer, const char* file_path) :
    m_renderer(renderer),
    m_scene(m_importer.ReadFile(file_path,
                               aiProcess_CalcTangentSpace       |
                               aiProcess_Triangulate            |
                               aiProcess_JoinIdenticalVertices  |
                               aiProcess_SortByPType))
{
    std::string path(file_path);
    m_base_dir = path.substr(0, (path.find_last_of('/') + 1));

    if(m_base_dir.empty())
        m_base_dir = path.substr(0, (path.find_last_of('\\') + 1));
}

void ModelLoader::load_mesh(Mesh& mesh)
{
    std::vector<Vertex> vertices = get_vertices(m_scene->mMeshes[0]);
    std::vector<u32> indices = get_indices(m_scene->mMeshes[0]);

    mesh.vertex_buffer = m_renderer->create_buffer({
           .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
           .size = (u32)(sizeof(vertices[0]) * vertices.size()),
           .data = vertices.data()
   });

    mesh.index_count = indices.size();
    mesh.index_buffer = m_renderer->create_buffer({
          .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
          .size = (u32)(sizeof(indices[0]) * indices.size()),
          .data = indices.data()
    });
}

void ModelLoader::load_material(Material& material)
{
    aiString diffuse_texture_path, specular_texture_path, normal_texture_path, occlusion_texture_path;
    m_scene->mMaterials[0]->GetTexture(aiTextureType_DIFFUSE, 0, &diffuse_texture_path);

    // don't bother to check other textures if no base colour is found
    if (diffuse_texture_path.length == 0)
    {
        std::string textures[] = {
            "../resources/textures/white_on_white.jpeg",
            "none",
            "none",
            "none"
        };

        material.texture = m_renderer->create_texture({
            .format = vk::Format::eR8G8B8A8Srgb,
            .image_src = textures[0].c_str()
        });

        material.sampler = m_renderer->create_sampler({
            .min_filter = vk::Filter::eLinear,
            .mag_filter = vk::Filter::eLinear,
            .u_mode = vk::SamplerAddressMode::eRepeat,
            .v_mode = vk::SamplerAddressMode::eRepeat,
            .w_mode = vk::SamplerAddressMode::eRepeat
        });

        material.descriptor_set = m_renderer->create_descriptor_set({
            .resource_handles = { material.texture },
            .sampler_handles = { material.sampler },
            .bindings = {0},
            .types = {vk::DescriptorType::eCombinedImageSampler},
            .layout = m_renderer->get_texture_layout(),
            .num_resources = 1
        });
        return;
    }

    m_scene->mMaterials[0]->GetTexture(aiTextureType_SPECULAR, 0, &specular_texture_path);
    m_scene->mMaterials[0]->GetTexture(aiTextureType_NORMALS, 0, &normal_texture_path);
    m_scene->mMaterials[0]->GetTexture(aiTextureType_AMBIENT, 0, &occlusion_texture_path);

    std::string textures[4];

    textures[0] = m_base_dir + diffuse_texture_path.C_Str();
    textures[1] = (specular_texture_path.length > 0) ? m_base_dir + specular_texture_path.C_Str() : "none";
    textures[2] = (normal_texture_path.length > 0) ? m_base_dir + normal_texture_path.C_Str() : "none";
    textures[3] = (occlusion_texture_path.length > 0) ? m_base_dir + occlusion_texture_path.C_Str() : "none";

    material.texture = m_renderer->create_texture({
        .format = vk::Format::eR8G8B8A8Srgb,
        .image_src = textures[0].c_str()
    });

    material.sampler = m_renderer->create_sampler({
        .min_filter = vk::Filter::eLinear,
        .mag_filter = vk::Filter::eLinear,
        .u_mode = vk::SamplerAddressMode::eRepeat,
        .v_mode = vk::SamplerAddressMode::eRepeat,
        .w_mode = vk::SamplerAddressMode::eRepeat
    });

    material.descriptor_set = m_renderer->create_descriptor_set({
        .resource_handles = { material.texture },
        .sampler_handles = { material.sampler },
        .bindings = {0},
        .types = {vk::DescriptorType::eCombinedImageSampler},
        .layout = m_renderer->get_texture_layout(),
        .num_resources = 1
    });
}

const char* ModelLoader::get_name()
{
    return m_scene->mMeshes[0]->mName.C_Str();
}

void ModelLoader::load_primitive(Renderer* renderer, PrimitiveTypes primitive, Mesh& mesh)
{
    std::vector<Vertex> vertices;
    std::vector<u32> indices;

    switch (primitive)
    {
        case PrimitiveTypes::None:
            return;
        case PrimitiveTypes::Cube:
        {
            vertices = Cube::vertices;
            indices = Cube::indices;
            break;
        }
        case PrimitiveTypes::Quad:
        {
            vertices = Quad::vertices;
            indices = Quad::indices;
            break;
        }
    }

    mesh.vertex_buffer = renderer->create_buffer({
        .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .size = (u32)(sizeof(vertices[0]) * vertices.size()),
        .data = vertices.data()
    });

    mesh.index_count = indices.size();
    mesh.index_buffer = renderer->create_buffer({
        .usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
        .size = (u32)(sizeof(indices[0]) * indices.size()),
        .data = indices.data()
    });
}

void ModelLoader::load_texture(Renderer* renderer, const char* texture_path, Material& material)
{
    material.texture = renderer->create_texture({
        .format = vk::Format::eR8G8B8A8Srgb,
        .image_src = texture_path
    });

    material.sampler = renderer->create_sampler({
        .min_filter = vk::Filter::eLinear,
        .mag_filter = vk::Filter::eLinear,
        .u_mode = vk::SamplerAddressMode::eRepeat,
        .v_mode = vk::SamplerAddressMode::eRepeat,
        .w_mode = vk::SamplerAddressMode::eRepeat
    });

    material.descriptor_set = renderer->create_descriptor_set({
        .resource_handles = { material.texture },
        .sampler_handles = { material.sampler },
        .bindings = {0},
        .types = {vk::DescriptorType::eCombinedImageSampler},
        .layout = renderer->get_texture_layout(),
        .num_resources = 1
    });
}

std::vector<Vertex> ModelLoader::get_vertices(aiMesh* mesh)
{
    std::vector<Vertex> vertices;
    for(int i = 0; i < mesh->mNumVertices; ++i)
    {
        vertices.push_back({
            .position   = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z },
            .normals    = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z },
            .uv         = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y }
        });
    }

    return vertices;
}

std::vector<unsigned> ModelLoader::get_indices(aiMesh* mesh)
{
    std::vector<unsigned> indices;
    for(int i = 0; i < mesh->mNumFaces; ++i)
    {
        for(int j = 0; j < mesh->mFaces[i].mNumIndices; ++j)
        {
            indices.push_back(mesh->mFaces[i].mIndices[j]);
        }
    }

    return indices;
}