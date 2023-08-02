#version 450

/*----------Textures----------*/
layout(set = 1, binding = 0) uniform sampler2D diffuse_sampler;
layout(set = 1, binding = 1) uniform sampler2D specular_sampler;
layout(set = 1, binding = 2) uniform sampler2D normal_sampler;
layout(set = 1, binding = 3) uniform sampler2D occlusion_sampler;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_tex_coord;

layout(location = 0) out vec4 out_colour;

vec4 light_colour = vec4(1.0, 1.0, 1.0, 1.0);

void main()
{
    vec4 diffuse_colour;
    vec4 specular_factor;
    vec3 normal;

    if(textureSize(diffuse_sampler, 0).x > 0)
    {
        diffuse_colour = texture(diffuse_sampler, v_tex_coord);
    }
    else
    {
        diffuse_colour = vec4(1.0, 0.0, 0.0, 1.0);
    }

    if(textureSize(normal_sampler, 0).x > 0)
    {
        normal = vec3(texture(normal_sampler, v_tex_coord));
    }
    else
    {
        normal = v_normal;
    }

    out_colour = diffuse_colour;
}