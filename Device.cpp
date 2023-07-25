#include "Device.hpp"

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

Device::Device(const vk::Instance& instance, vk::PhysicalDeviceFeatures features, vk::DeviceCreateInfo create_info)
{

}

void Device::pick_physical_device(const vk::Instance& instance)
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
        m_physical_device = candidates.rbegin()->second;
    }
    else
    {
        throw std::runtime_error("failed to find a suitable GPU!");
    }
}

bool Device::is_device_suitable(vk::PhysicalDevice device)
{
//    QueueFamilyIndices indices = find_queue_families(device);
//
//    bool surface_supported = device.getSurfaceSupportKHR(indices.graphics_family.value(), m_surface);
//
//    if(!surface_supported) return false;

    return check_device_extension_support(device);
//    bool swap_chain_adequate = false;
//
//    if (extensions_supported)
//    {
//        SwapChainSupportDetails swap_chain_support = query_swap_chain_support(device);
//
//        swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
//    }

    //return  /* indices.is_complete()  && */ extensions_supported && swap_chain_adequate;
}

int Device::rate_device_suitability(vk::PhysicalDevice device)
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

QueueFamilyIndices find_queue_families(vk::PhysicalDevice device, vk::SurfaceKHR surface)
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

bool Device::check_device_extension_support(vk::PhysicalDevice device)
{
    unsigned extension_count = 0;
    vk::Result result = device.enumerateDeviceExtensionProperties(nullptr, &extension_count, nullptr);

    std::vector<vk::ExtensionProperties> available_extensions(extension_count);
    result = device.enumerateDeviceExtensionProperties(nullptr, &extension_count, available_extensions.data());

    std::set<std::string> required_extensions(m_device_extensions.begin(), m_device_extensions.end());

    for(const auto& extension : available_extensions)
    {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}