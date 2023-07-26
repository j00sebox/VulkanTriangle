#include "config.hpp"

struct Buffer
{
    vk::Buffer buffer;
    vk::DeviceMemory memory;
};

struct Mesh
{
    Buffer vertex_buffer;
    Buffer index_buffer;
    u32 index_count;
};