#version 450

layout(location = 0) in vec3 frag_normal;
layout(location = 1) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_colour;

layout(set = 1, binding = 0) uniform sampler2D tex_sampler;

void main()
{
    out_colour = texture(tex_sampler, frag_tex_coord);
}