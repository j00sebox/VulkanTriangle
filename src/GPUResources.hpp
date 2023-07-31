#pragma once
#include "config.hpp"
#include <vk_mem_alloc.h>

struct Buffer
{
    VkBuffer                        vk_buffer;
    VmaAllocation                   vma_allocation;

    u32                             size = 0;
};

struct Texture
{
    VkImage                         vk_image;
    VkImageView                     vk_image_view;
    VkFormat                        vk_format;
    VkImageLayout                   vk_image_layout;
    VmaAllocation                   vma_allocation;

    u16                             width = 1;
    u16                             height = 1;
    u16                             depth = 1;
    u8                              mipmaps = 1;
};

struct Sampler
{
    VkSampler                       vk_sampler;

    VkFilter                        min_filter = VK_FILTER_NEAREST;
    VkFilter                        mag_filter = VK_FILTER_NEAREST;
    VkSamplerMipmapMode             mip_filter = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VkSamplerAddressMode            address_mode_u = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode            address_mode_v = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    VkSamplerAddressMode            address_mode_w = VK_SAMPLER_ADDRESS_MODE_REPEAT;
};

struct BufferCreationInfo
{
    vk::BufferUsageFlags            usage;
    u32                             size;
    void*                           data;
};

struct TextureCreationInfo
{
    vk::Format                        format          = vk::Format::eUndefined;

    const char*                     image_src;
};