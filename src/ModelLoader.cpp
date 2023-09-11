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

Model ModelLoader::load()
{
    Model model;

    Mesh mesh{};
    Material material{};
    load_mesh(mesh);
    load_material(material);

    model.meshes.push_back(mesh);
    model.materials.push_back(material);

    return model;
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

        material.textures[0] = m_renderer->create_texture({
            .format = vk::Format::eR8G8B8A8Srgb,
            .image_src = textures[0].c_str()
        });

        material.textures[1] = m_renderer->get_null_texture_handle();
        material.textures[2] = m_renderer->get_null_texture_handle();
        material.textures[3] = m_renderer->get_null_texture_handle();

        // FIXME
        u32 texture_handles[] = { material.textures[0], m_renderer->get_null_texture_handle(), m_renderer->get_null_texture_handle(), m_renderer->get_null_texture_handle() };
        m_renderer->update_texture_set(texture_handles, 4);

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

    material.textures[0] = m_renderer->create_texture({
        .format = vk::Format::eR8G8B8A8Srgb,
        .image_src = textures[0].c_str()
    });

    for(int i = 1; i < 4; ++i)
    {
        if(textures[i] != "none")
        {
            material.textures[i] = m_renderer->create_texture({
                .format = vk::Format::eR8G8B8A8Srgb,
                .image_src = textures[i].c_str()
            });
        }
        else
        {
            material.textures[i] = m_renderer->get_null_texture_handle();
        }
    }

    // FIXME
    u32 texture_handles[] = { material.textures[0], material.textures[1], material.textures[2], material.textures[3] };
    m_renderer->update_texture_set(texture_handles, 4);
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

// FIXME: loading single texture should not create a descriptor set
void ModelLoader::load_texture(Renderer* renderer, const char* texture_path, Material& material)
{
    material.textures[0] = renderer->create_texture({
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
        .resource_handles = { material.textures[0], renderer->get_null_texture_handle(), renderer->get_null_texture_handle(), renderer->get_null_texture_handle() },
        .sampler_handles = { material.sampler, material.sampler, material.sampler, material.sampler },
        .bindings = {0, 1, 2, 3},
        .types = {vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eCombinedImageSampler, vk::DescriptorType::eCombinedImageSampler},
        .layout = renderer->get_texture_layout(),
        .num_resources = 4
    });
}

std::vector<Vertex> ModelLoader::get_vertices(aiMesh* mesh)
{
    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
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