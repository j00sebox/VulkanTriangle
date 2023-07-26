#pragma once

#include "config.hpp"
#include "Vertex.hpp"
#include "GPUResources.hpp"

class Device
{
public:
    Device(const vk::Instance& instance, const vk::SurfaceKHR& surface, vk::DeviceCreateInfo create_info, vk::Extent2D extent);
    ~Device();

    void draw_frame(u32 current_frame);
    void recreate_swapchain(vk::Extent2D extent);

    // resource creation
    void create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& image_memory);
    void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags  properties, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    void create_vertex_buffer(const std::vector<Vertex>& vertices);
    void create_index_buffer(const std::vector<u32>& indices);
    vk::ImageView create_image_view(const vk::Image& image, vk::Format format, vk::ImageAspectFlags image_aspect);
    vk::ShaderModule create_shader_module(const std::vector<char>& code);

    void wait_for_idle() { m_logical_device.waitIdle(); }

private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_logical_device;
    const vk::SurfaceKHR* m_surface;

    // swapchain related things
    vk::SwapchainKHR m_swapchain;
    vk::Format m_swapchain_image_format;
    vk::Extent2D m_swapchain_extent;
    std::vector<vk::Image> m_swapchain_images;
    std::vector<vk::ImageView> m_swapchain_image_views;
    std::vector<vk::Framebuffer> m_swapchain_framebuffers;
    vk::RenderPass m_render_pass;
    vk::DescriptorSetLayout m_descriptor_set_layout;
    vk::DescriptorPool m_descriptor_pool;
    std::vector<vk::DescriptorSet> m_descriptor_sets;
    vk::PipelineLayout m_pipeline_layout;
    vk::Pipeline m_graphics_pipeline;

    // command pools manage the memory that is used to store the buffers and command buffers are allocated to them
    vk::CommandPool m_command_pool;

    // each frame need its own command buffer, semaphores and fence
    std::vector<vk::CommandBuffer> m_command_buffers;

    // we want to use semaphores for swapchain operations since they happen on the GPU
    std::vector<vk::Semaphore> m_image_available_semaphores;
    std::vector<vk::Semaphore> m_render_finished_semaphores;

    // purpose is to order execution on the CPU
    // if the host need to know when the GPU had finished we need a fence
    // in signaled/unsignaled state
    std::vector<vk::Fence> m_in_flight_fences;

    // queues
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;

    // depth buffer
    vk::Image m_depth_image;
    vk::DeviceMemory m_depth_image_memory;
    vk::ImageView m_depth_image_view;

    // TODO: remove
    vk::Image m_texture_image;
    vk::DeviceMemory m_texture_image_memory;
    vk::ImageView m_texture_image_view;
    vk::Sampler m_texture_sampler;
    const std::string TEXTURE_PATH = "../textures/viking_room.png";

    // TODO: remove
    vk::Buffer m_vertex_buffer;
    vk::DeviceMemory m_vertex_buffer_memory;
    vk::Buffer m_index_buffer;
    vk::DeviceMemory m_index_buffer_memory;
    u32 index_count;

    // uniform buffers
    std::vector<vk::Buffer> m_uniform_buffers;
    std::vector<vk::DeviceMemory> m_uniform_buffers_memory;
    std::vector<void*> m_uniform_buffers_mapped;

    void create_swapchain(vk::Extent2D extent);
    void create_render_pass();
    void create_descriptor_set_layout();
    void create_graphics_pipeline();
    void create_command_pool();
    void create_depth_resources();
    void create_framebuffers();
    void create_texture_image();
    void create_texture_image_view();
    void create_texture_sampler();
    void create_uniform_buffers();
    void create_descriptor_pool();
    void create_descriptor_sets();
    void create_command_buffer();
    void create_sync_objects();

    void update_uniform_buffer(unsigned current_image);
    void record_command_buffer(vk::CommandBuffer& command_buffer, unsigned image_index, u32 current_frame);

    void cleanup_swapchain();
    void copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size);
    void copy_buffer_to_image(vk::Buffer buffer, vk::Image image, u32 width, u32 height);
    void transition_image_layout(vk::Image image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout);

    vk::CommandBuffer begin_single_time_commands();
    void end_single_time_commands(vk::CommandBuffer command_buffer);
};

