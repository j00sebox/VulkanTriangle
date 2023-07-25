#pragma once

#include "config.hpp"

class Device
{
public:
    Device(const vk::Instance& instance, const vk::SurfaceKHR& surface, vk::DeviceCreateInfo create_info);
    ~Device();

    void draw_frame(u32 current_frame);

private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_logical_device;

    // queues
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;

    // required extensions the device should have
    const std::vector<const char *> m_device_extensions =
    {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    void pick_physical_device(const vk::Instance& instance);
    void create_device();
    bool is_device_suitable(vk::PhysicalDevice device);
    int rate_device_suitability(vk::PhysicalDevice device);
    bool check_device_extension_support(vk::PhysicalDevice device);
};

