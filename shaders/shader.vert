#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec2 v_tex_coord;
layout(location = 2) out vec3 camera_position;

layout(push_constant) uniform constants
{
    mat4 model;
} object_transform;

layout(set=0, binding=0) uniform CameraDataBuffer
{
    mat4 view;
    mat4 proj;
    vec3 camera_position;
} camera_data;

void main()
{
    // gl_VertexIndex contains the index of the current vertex
    gl_Position = camera_data.proj * camera_data.view * object_transform.model * vec4(in_position, 1.0);
    v_normal = transpose(inverse(mat3(object_transform.model))) * in_normal;
    v_tex_coord = in_uv;

    camera_position = vec3(0.0, 0.0, 0.0); // camera_data.camera_position; // FIXME ?
}