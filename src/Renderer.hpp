#pragma once
#include "config.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Scene.hpp"
#include "GPUResources.hpp"
#include "Components.hpp"
#include "OBJLoader.hpp"

class Renderer
{
public:
    explicit Renderer(GLFWwindow* window);
    ~Renderer();

    void render(Scene* scene);
    Model load_model(const OBJLoader& loader);
    void draw_frame(u32 current_frame, const Mesh& mesh);
    void start_renderpass(vk::CommandBuffer command_buffer, u32 image_index);
    void end_renderpass(vk::CommandBuffer command_buffer);
    void recreate_swapchain();

    // resource creation
    void create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& image_memory);
    void create_buffer(const BufferCreationInfo& buffer_creation, Buffer& buffer);
    void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags  properties, vk::Buffer& buffer, vk::DeviceMemory& buffer_memory);
    vk::ImageView create_image_view(const vk::Image& image, vk::Format format, vk::ImageAspectFlags image_aspect);
    vk::ShaderModule create_shader_module(const std::vector<char>& code);

    void destroy_buffer(Buffer& buffer);

    void wait_for_device_idle() { m_logical_device.waitIdle(); }

private:
    GLFWwindow* m_window;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger;
    vk::DispatchLoaderDynamic m_dispatch_loader;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physical_device;
    vk::Device m_logical_device;
    VmaAllocator m_allocator;

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
    Buffer m_vertex_buffer;
    Buffer m_index_buffer;
    u32 index_count;

    // uniform buffers
    std::vector<vk::Buffer> m_uniform_buffers;
    std::vector<vk::DeviceMemory> m_uniform_buffers_memory;
    std::vector<void*> m_uniform_buffers_mapped;

    void create_instance();
    void create_surface();
    void create_device();
    void create_swapchain();
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

    // allow multiple frames to be in-flight
    // this means we allow a new frame to start being rendered without interfering with one being presented
    // meaning we need multiple command buffers, semaphores and fences
    const u16 MAX_FRAMES_IN_FLIGHT = 2;

    // keeps track of the current frame index
    u32 m_current_frame = 0;

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

