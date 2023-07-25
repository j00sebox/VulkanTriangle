#pragma once

#include "config.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Device.hpp"

static void framebuffer_resize_callback(GLFWwindow* window, i32 width, i32 height);

class Engine
{
public:
    Engine(i32 width, i32 height, GLFWwindow* window);
    ~Engine();

    void render();

    // many drivers trigger the out of date result, it's not guaranteed
    bool m_framebuffer_resized = false;

private:
    GLFWwindow* m_window;
    i32 m_width, m_height;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger;
    vk::DispatchLoaderDynamic m_dispatch_loader;
    vk::SurfaceKHR m_surface;
    Device* m_device;
    vk::PhysicalDevice m_physical_device = nullptr;
    vk::Device m_logical_device;
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

    // queues are automatically created when the logical device is, but we need a handle to it,
    // they are also implicitly cleaned up when the device is destroyed
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;

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

    void create_instance();
    void create_surface();
    void create_device();

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

