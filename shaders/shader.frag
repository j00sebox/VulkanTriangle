#version 450

/*----------Textures----------*/
layout(set = 1, binding = 0) uniform sampler2D diffuse_sampler;
layout(set = 1, binding = 1) uniform sampler2D specular_sampler;
layout(set = 1, binding = 2) uniform sampler2D normal_sampler;
layout(set = 1, binding = 3) uniform sampler2D occlusion_sampler;

layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_tex_coord;
layout(location = 3) in vec3 camera_position;

layout(location = 0) out vec4 out_colour;

layout(set=0, binding=1) uniform LightDataBuffer
{
    vec4 ambient_colour;
    vec4 direct_light_colour;
    vec3 direct_light_position;
} light_data;

bool is_texture_valid(sampler2D texture)
{
    return(textureSize(texture, 0).x > 1);
}

void main()
{
    vec4 diffuse_colour;
    float specular_factor;
    vec3 normal;

    if(is_texture_valid(diffuse_sampler))
    {
        diffuse_colour = texture(diffuse_sampler, v_tex_coord);
    }
    else
    {
        diffuse_colour = vec4(1.0, 0.0, 0.0, 1.0);
    }

    if(is_texture_valid(normal_sampler))
    {
        normal = vec3(texture(normal_sampler, v_tex_coord));
    }
    else
    {
        normal = v_normal;
    }

    vec3 l = normalize(light_data.direct_light_position - v_position); // light direction
    vec3 v = normalize(camera_position - v_position); // view direction
    vec3 h = normalize(l + v); // halfway vector

    float shininess = 0.5;
    specular_factor = pow(max(dot(normal, h), 0.0), shininess);
    vec3 specular = vec3(light_data.direct_light_colour * specular_factor);

    out_colour = diffuse_colour * light_data.ambient_colour + vec4(specular, 1.0);
}