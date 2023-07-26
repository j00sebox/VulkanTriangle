#pragma once

#include "config.hpp"

// it is possible for the graphics and present family to not overlap
struct QueueFamilyIndices
{
    std::optional<unsigned> graphics_family;
    std::optional<unsigned> present_family;

    [[nodiscard]] bool is_complete() const { return (graphics_family.has_value() && present_family.has_value()); }
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

// required extensions the device should have
const std::vector<const char *> g_device_extensions =
{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

namespace DeviceHelper
{
    static QueueFamilyIndices find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    {
        QueueFamilyIndices indices;
        unsigned queue_family_count = 0;
        device.getQueueFamilyProperties(&queue_family_count, nullptr);

        if (queue_family_count == 0) return indices;

        std::vector<vk::QueueFamilyProperties> queue_families(queue_family_count);
        device.getQueueFamilyProperties(&queue_family_count, queue_families.data());

        int i = 0;
        for (const auto &queue_family: queue_families) {
            if (queue_family.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                indices.graphics_family = i;
            }

            // it is very likely that these will be the same family
            // can add logic to prefer queue families that contain both
            vk::Bool32 present_support = false;
            vk::Result result = device.getSurfaceSupportKHR(i, surface, &present_support);

            if (present_support)
            {
                indices.present_family = i;
            }

            if (indices.is_complete()) break;

            ++i;
        }

        return indices;
    }

    static bool check_device_extension_support(vk::PhysicalDevice device)
    {
        unsigned extension_count = 0;
        vk::Result result = device.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr);

        std::vector<vk::ExtensionProperties> available_extensions(extension_count);
        result = device.enumerateDeviceExtensionProperties(nullptr, &extension_count, available_extensions.data());

        std::set<std::string> required_extensions(g_device_extensions.begin(), g_device_extensions.end());

        for(const auto& extension : available_extensions)
        {
            required_extensions.erase(extension.extensionName);
        }

        return required_extensions.empty();
    }

    bool is_device_suitable(vk::PhysicalDevice device)
    {
        return check_device_extension_support(device);
    }

    static int rate_device_suitability(vk::PhysicalDevice device)
    {
        int score = 0;
        vk::PhysicalDeviceProperties device_properties;
        vk::PhysicalDeviceFeatures device_features;

        device.getProperties(&device_properties);
        device.getFeatures(&device_features);

        // large performance advantage
        if (device_properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
        {
            score += 1000;
        }

        // max possible size of textures
        score += device_properties.limits.maxImageDimension2D;

        // no geometry shader, no deal Howie
        if (!device_features.geometryShader) {
            return 0;
        }

        return score;
    }

    vk::PhysicalDevice pick_physical_device(const vk::Instance& instance)
    {
        unsigned device_count = 0;
        vk::Result result = instance.enumeratePhysicalDevices(&device_count, nullptr);

        if (device_count == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<vk::PhysicalDevice> devices(device_count);

        // keeps track of devices and their score to pick the best option
        std::multimap<int, vk::PhysicalDevice> candidates;
        result = instance.enumeratePhysicalDevices(&device_count, devices.data());

        for (const auto& device: devices)
        {
            if (is_device_suitable(device))
            {
                int score = rate_device_suitability(device);
                candidates.insert(std::make_pair(score, device));
            }
        }

        if (candidates.rbegin()->first > 0)
        {
            return candidates.rbegin()->second;
        }
        else
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    SwapChainSupportDetails query_swap_chain_support(vk::PhysicalDevice device, vk::SurfaceKHR surface)
    {
        SwapChainSupportDetails details;

        vk::Result result = device.getSurfaceCapabilitiesKHR(surface, &details.capabilities);

        unsigned format_count = 0;
        result = device.getSurfaceFormatsKHR(surface, &format_count, nullptr);

        if (format_count != 0)
        {
            details.formats.resize(format_count);
            result = device.getSurfaceFormatsKHR(surface, &format_count, details.formats.data());
        }

        unsigned present_mode_count = 0;
        result = device.getSurfacePresentModesKHR(surface, &present_mode_count, nullptr);

        if (present_mode_count != 0)
        {
            details.present_modes.resize(present_mode_count);
            result = device.getSurfacePresentModesKHR(surface, &present_mode_count, details.present_modes.data());
        }

        return details;
    }

    vk::SurfaceFormatKHR choose_swap_surface_format(const std::vector<vk::SurfaceFormatKHR>& available_formats)
    {
        for(const auto& available_format : available_formats)
        {
            if(available_format.format == vk::Format::eB8G8R8A8Srgb && available_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return available_format;
            }
        }

        return available_formats[0];
    }

    // represents the conditions of putting an image on the screen
    vk::PresentModeKHR choose_swap_present_mode(const std::vector<vk::PresentModeKHR>& available_present_modes)
    {
        for(const auto& present_mode : available_present_modes)
        {
            // triple buffering
            if(present_mode == vk::PresentModeKHR::eMailbox)
            {
                return present_mode;
            }
        }

        // most similar to regular v-sync
        return vk::PresentModeKHR::eFifo;
    }

    // resolution of the swap chain images
    // range of possible resolutions is defined in the vk::SurfaceCapabilitiesStructure
    // normally we would set the resolution of the window to the currentExtent member
    // GLFW works in terms of screen coordinates whereas Vulkan uses pixels
    vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D extent)
    {
        if(capabilities.currentExtent.width != std::numeric_limits<unsigned>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            // bound extent within limits set by the implementation
            extent.width = std::clamp(extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            extent.height = std::clamp(extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return extent;
        }
    }

    u32 find_memory_type(u32 type_filter, vk::MemoryPropertyFlags properties, vk::PhysicalDevice physical_device)
    {
        // has two arrays, memoryTpyes and memoryHeaps
        // the heaps are distinct memory sources like VRAM and swap space in RAM
        vk::PhysicalDeviceMemoryProperties memory_properties;
        physical_device.getMemoryProperties(&memory_properties);

        // find suitable memory type
        for(unsigned i = 0; i < memory_properties.memoryTypeCount; ++i)
        {
            // type filter represent the bits of what is suitable
            // we also need to be able to write vertex data to that memory
            if((type_filter & (1 << i)) && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        return 0;
    }
}