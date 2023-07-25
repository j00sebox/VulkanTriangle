#version 450

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_colour;

layout(binding = 1) uniform sampler2D tex_sampler;

void main()
{
    out_colour = texture(tex_sampler, frag_tex_coord); // vec4(frag_tex_coord, 0.0, 1.0);
}