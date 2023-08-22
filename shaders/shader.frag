#version 450
#extension GL_EXT_nonuniform_qualifier : enable

/*----------Textures----------*/
layout(set = 1, binding = 10) uniform sampler2D textures[];

layout(push_constant) uniform texture_info
{
    layout(offset = 64) uvec4 texture_indices;
};

/*----------Vertex Attributes----------*/
layout(location = 0) in vec3 v_position;
layout(location = 1) in vec3 v_normal;
layout(location = 2) in vec2 v_tex_coord;

layout(location = 0) out vec4 out_colour;

layout(set=0, binding=0) uniform CameraDataBuffer
{
    mat4 view;
    mat4 proj;
    vec3 camera_position;
} camera_data;

layout(set=0, binding=1) uniform LightDataBuffer
{
    vec3 direct_light_colour;
    vec3 direct_light_position;
} light_data;

bool is_texture_valid(sampler2D texture)
{
    return(textureSize(texture, 0).x > 1);
}

void main()
{
    vec3 base_colour;
    float specular_factor;
    vec3 normal;

    if(is_texture_valid(textures[nonuniformEXT(texture_indices.x)]))
    {
        base_colour = texture(textures[nonuniformEXT(texture_indices.x)], v_tex_coord).rgb;
    }
    else
    {
        base_colour = vec3(1.0, 0.0, 0.0);
    }

    if(is_texture_valid(textures[texture_indices.z]))
    {
        normal = vec3(texture(textures[texture_indices.z], v_tex_coord));
    }
    else
    {
        normal = v_normal;
    }

    vec3 ambient = 0.05 * light_data.direct_light_colour;

    vec3 l = normalize(light_data.direct_light_position - v_position); // light direction
    vec3 v = normalize(camera_data.camera_position - v_position); // view direction
    vec3 h = normalize(l + v); // halfway vector

    vec3 diffuse = max(dot(normal, l), 0.0) * light_data.direct_light_colour;

    float shininess = 32.0;
    float strength = 0.2;
    specular_factor = pow(max(dot(normal, h), 0.0), shininess);
    vec3 specular = light_data.direct_light_colour * specular_factor * strength;

    vec3 result = (diffuse + ambient + specular) * base_colour;
    out_colour = vec4(result, 1.0);
}