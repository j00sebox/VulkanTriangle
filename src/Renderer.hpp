#pragma once
#include "config.hpp"
#include "Scene.hpp"
#include "GPUResources.hpp"
#include "Memory.hpp"
#include "Components.hpp"
#include "CommandBuffer.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <TaskScheduler.h>

struct LightingData
{
    glm::vec3 direct_light_colour;
    glm::vec3 direct_light_position;
};

class Renderer
{
public:
    explicit Renderer(GLFWwindow* window, enki::TaskScheduler* scheduler);
    ~Renderer();

    void render(Scene* scene);
    void begin_frame();
    void end_frame();
    void recreate_swapchain();

    // resource creation
    u32 create_buffer(const BufferCreationInfo& buffer_creation);
    u32 create_texture(const TextureCreationInfo& texture_creation);
    u32 create_sampler(const SamplerCreationInfo& sampler_creation);
    u32 create_descriptor_set(const DescriptorSetCreationInfo& descriptor_set_creation);
    void create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& image_memory);
    void create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, VmaAllocation& image_vma);
    vk::ImageView create_image_view(const vk::Image& image, vk::Format format, vk::ImageAspectFlags image_aspect);
    vk::ShaderModule create_shader_module(const std::vector<char>& code);

    Buffer* get_buffer(u32 buffer_handle) { return static_cast<Buffer*>(m_buffer_pool.access(buffer_handle)); }
    DescriptorSet* get_descriptor_set(u32 descriptor_set_handle) { return static_cast<DescriptorSet*>(m_descriptor_set_pool.access(descriptor_set_handle)); }

    void update_texture_set(u32* texture_handles, u32 num_textures);

    void destroy_buffer(u32 buffer_handle);
	void destroy_texture(u32 texture_handle);
	void destroy_sampler(u32 sampler_handle);
    void configure_lighting(LightingData data);

	[[nodiscard]] LightingData get_light_data() const { return m_light_data; }
    void wait_for_device_idle() const { logical_device.waitIdle(); }

    [[nodiscard]] const vk::DescriptorSetLayout& get_texture_layout() const { return m_texture_set_layout; }
	[[nodiscard]] u32 get_null_texture_handle() const { return m_null_texture; }
	const vk::PipelineLayout& get_pipeline_layout() { return m_pipeline_layout; }

    // allow multiple frames to be in-flight
    // this means we allow a new frame to start being rendered without interfering with one being presented
    // meaning we need multiple command buffers, semaphores and fences
    static const u16 s_max_frames_in_flight = 3;

    vk::Device logical_device;

private:
    GLFWwindow* m_window;
    enki::TaskScheduler* m_scheduler;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger;
    vk::DispatchLoaderDynamic m_dispatch_loader;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physical_device;
    vk::PhysicalDeviceProperties m_device_properties;
    VmaAllocator m_allocator;
    PoolAllocator m_pool_allocator;

    // swapchain related things
    vk::SwapchainKHR m_swapchain;
    vk::Format m_swapchain_image_format;
    vk::Extent2D m_swapchain_extent;
    std::vector<vk::Image> m_swapchain_images;
    std::vector<vk::ImageView> m_swapchain_image_views;
    std::vector<vk::Framebuffer> m_swapchain_framebuffers;
    vk::RenderPass m_render_pass;
    vk::DescriptorSetLayout m_descriptor_set_layout;
    vk::DescriptorSetLayout m_camera_data_layout;
    vk::DescriptorSetLayout m_texture_set_layout;
    vk::DescriptorPool m_descriptor_pool;
    vk::DescriptorPool m_imgui_pool;
    std::vector<vk::DescriptorSet> m_descriptor_sets;
    std::vector<u32> m_camera_sets;
    u32 m_texture_set;
    vk::PipelineLayout m_pipeline_layout;
    vk::Pipeline m_graphics_pipeline;

    u32 m_image_index;

    // command pools manage the memory that is used to store the buffers and command buffers are allocated to them
    vk::CommandPool m_main_command_pool;
    vk::CommandPool m_extra_command_pool;
    std::vector<vk::CommandPool> m_command_pools;

    // each frame need its own command buffer, semaphores and fence
    std::array<CommandBuffer, s_max_frames_in_flight> m_primary_command_buffers;
    std::vector<vk::CommandBuffer> m_command_buffers;
    std::array<vk::CommandBuffer, s_max_frames_in_flight> m_extra_draw_commands;
    std::array<CommandBuffer, s_max_frames_in_flight> m_imgui_commands;

    // we want to use semaphores for swapchain operations since they happen on the GPU
    std::array<vk::Semaphore, s_max_frames_in_flight> m_image_available_semaphores;
    std::array<vk::Semaphore, s_max_frames_in_flight> m_render_finished_semaphores;

    // purpose is to order execution on the CPU
    // if the host need to know when the GPU had finished we need a fence
    // in signaled/unsignaled state
    std::array<vk::Fence, s_max_frames_in_flight> m_in_flight_fences;

    // queues
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;
    vk::Queue m_transfer_queue;

    // depth buffer
    vk::Image m_depth_image;
    vk::DeviceMemory m_depth_image_memory;
    vk::ImageView m_depth_image_view;

    ResourcePool m_buffer_pool;
    ResourcePool m_texture_pool;
    ResourcePool m_sampler_pool;
    ResourcePool m_descriptor_set_pool;

    // uniform buffers
    std::array<u32, s_max_frames_in_flight> m_camera_buffers;
    std::array<u32, s_max_frames_in_flight> m_light_buffers;

    LightingData m_light_data;

    // texture used when loader can't find one
    u32 m_null_texture;

    // for most texture use
    u32 m_default_sampler;

    void init_instance();
    void init_surface();
    void init_device();
    void init_swapchain();
    void init_render_pass();
    void init_graphics_pipeline();
    void init_command_pools();
    void init_depth_resources();
    void init_framebuffers();
    void init_descriptor_pools();
    void init_descriptor_sets();
    void init_command_buffers();
    void init_sync_objects();
    void init_imgui();

    void cleanup_swapchain();
    void copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size);
    void copy_buffer_to_image(vk::Buffer buffer, vk::Image image, u32 width, u32 height);
    void transition_image_layout(vk::Image image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout);
    [[nodiscard]] size_t pad_uniform_buffer(size_t original_size) const;

    vk::CommandBuffer begin_single_time_commands();
    void end_single_time_commands(vk::CommandBuffer command_buffer);

    std::map<std::string, u32> m_texture_map;

    // keeps track of the current frame index
    u32 m_current_frame = 0;

    // index of command buffer that is currently being written to
    u32 m_current_cb_index = 0;

#ifdef NDEBUG
    const bool m_enable_validation_layers = false;
#else
    const bool m_enable_validation_layers = true;
#endif

    const std::vector<const char *> m_validation_layers =
    {
            "VK_LAYER_KHRONOS_validation" // default one with SDK
    };

    bool check_validation_layer_support();
    [[nodiscard]] std::vector<const char*> get_required_extensions() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
    )
    {
        std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }
};

