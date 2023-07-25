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

Device::Device(const vk::Instance& instance, const vk::SurfaceKHR& surface, vk::DeviceCreateInfo create_info)
{
    pick_physical_device(instance);
    QueueFamilyIndices indices = find_queue_families(m_physical_device, surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set<unsigned> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};

    float queue_priority = 1.f;
    for (unsigned queue_family: unique_queue_families) {
        vk::DeviceQueueCreateInfo queue_create_info{};

        // described number of queues we want in queue family
        queue_create_info.sType = vk::StructureType::eDeviceQueueCreateInfo;
        queue_create_info.queueFamilyIndex = queue_family;

        // drivers will only allow a small # of queues per queue family, but you don't need more than 1 really
        // this is because you can create the command buffers on separate threads and submit all at once
        queue_create_info.queueCount = 1;

        // you can assign priorities to influence scheduling of command buffer execution
        // number between 0.0 and 1.0
        // this is required even if only 1 queue
        queue_create_info.pQueuePriorities = &queue_priority;

        queue_create_infos.push_back(queue_create_info);
    }

    create_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();

    // previous versions of Vulkan had a distinction between instance and device specific validation layers
    // meaning enabledLayerCount and ppEnabledLayerNames field are ignored
    // it is still a good idea to set them to be compatible with older versions
    create_info.enabledExtensionCount = static_cast<unsigned>(m_device_extensions.size());
    create_info.ppEnabledExtensionNames = m_device_extensions.data();

    // takes physical device to interface with, the queue and the usage info
    if (m_physical_device.createDevice(&create_info, nullptr, &m_logical_device) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    // since we only set 1 queue we can just index at 0
    m_logical_device.getQueue(indices.graphics_family.value(), 0, &m_graphics_queue);
    m_logical_device.getQueue(indices.present_family.value(), 0, &m_present_queue);
}

Device::~Device()
{
    // devices don't interact directly with instances
    m_logical_device.destroy();
}

void Device::draw_frame(u32 current_frame)
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