#pragma once
#include "config.hpp"

#include <vk_mem_alloc.h>

const u32 k_max_bindless_resources = 1024;

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
    vk::Sampler                       vk_sampler;

    vk::Filter                        min_filter;
    vk::Filter                        mag_filter;

    vk::SamplerAddressMode            address_mode_u;
    vk::SamplerAddressMode            address_mode_v;
    vk::SamplerAddressMode            address_mode_w;
};

struct DescriptorSet
{
    vk::DescriptorSet                 vk_descriptor_set;

    u32*                            resources = nullptr;
    u32*                            samplers = nullptr;
    u16*                            bindings = nullptr;

    u32                             num_resources;
};

struct BufferCreationInfo
{
    vk::BufferUsageFlags            usage;
    u32                             size;
    void*                           data;
};

struct TextureCreationInfo
{
    vk::Format                      format          = vk::Format::eUndefined;

    const char*                     image_src;
};

struct SamplerCreationInfo
{
    vk::Filter                      min_filter;
    vk::Filter                      mag_filter;

    vk::SamplerAddressMode          u_mode;
    vk::SamplerAddressMode          v_mode;
    vk::SamplerAddressMode          w_mode;
};

// FIXME: magic numbers
struct DescriptorSetLayoutCreationInfo
{
    struct Binding
    {

        vk::DescriptorType          type    = vk::DescriptorType::eMutableVALVE;
        u16                         start   = 0;
        u16                         count   = 0;
    };

    Binding                         bindings[16];
    u32                             num_bindings    = 0;
    u32                             set_index       = 0;
};

// FIXME: naughty magic numbers
struct DescriptorSetCreationInfo
{
    u32                             resource_handles[8]{};
    u32                             sampler_handles[8]{};
    u16                             bindings[8]{};
    vk::DescriptorType              types[8]{};

    vk::DescriptorSetLayout         layout;
    u32                             num_resources{};
};