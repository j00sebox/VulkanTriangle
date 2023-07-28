#pragma once
#include "config.hpp"
#include <vk_mem_alloc.h>

struct Buffer
{
    VkBuffer vk_buffer;
    VmaAllocation vma_allocation;
    u32 size;
};

struct BufferCreationInfo
{
    u32 size;
    vk::BufferUsageFlags usage;
    void* data;
};