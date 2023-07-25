#pragma once

#include "config.hpp"

class Device
{
public:
    Device(const vk::Instance& instance, const vk::SurfaceKHR& surface, vk::DeviceCreateInfo create_info, vk::Extent2D extent);
    ~Device();

    void draw_frame(u32 current_frame);

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

    // queues
    vk::Queue m_graphics_queue;
    vk::Queue m_present_queue;

    void create_swapchain(vk::Extent2D extent);
    void cleanup_swapchain();
};

