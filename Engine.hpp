#pragma once

#include "config.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "OBJLoader.hpp"
#include "Device.hpp"

class Engine
{
public:
    Engine(i32 width, i32 height, GLFWwindow* window);
    ~Engine();

    void render();
    void load_model(const OBJLoader& loader);
    void wait_for_device_idle();

private:
    GLFWwindow* m_window;
    i32 m_width, m_height;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger;
    vk::DispatchLoaderDynamic m_dispatch_loader;
    vk::SurfaceKHR m_surface;
    Device* m_device;

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

