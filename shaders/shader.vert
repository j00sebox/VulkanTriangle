#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;

layout(location = 0) out vec2 frag_tex_coord;

layout(push_constant) uniform constants
{
    mat4 model;
} object_transform;

layout(set=0, binding=0) uniform UniformBufferObject
{
    mat4 view;
    mat4 proj;
} ubo;

void main()
{
    // gl_VertexIndex contains the index of the current vertex
    gl_Position = ubo.proj * ubo.view * object_transform.model * vec4(in_position, 1.0);
    frag_tex_coord = in_uv;
}