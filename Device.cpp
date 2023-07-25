#include "Device.hpp"
#include "DeviceHelper.hpp"

Device::Device(const vk::Instance& instance, const vk::SurfaceKHR& surface, vk::DeviceCreateInfo create_info, vk::Extent2D extent) :
    m_surface(&surface)
{
    m_physical_device = DeviceHelper::pick_physical_device(instance);
    QueueFamilyIndices indices = DeviceHelper::find_queue_families(m_physical_device, surface);

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
    create_info.enabledExtensionCount = static_cast<unsigned>(g_device_extensions.size());
    create_info.ppEnabledExtensionNames = g_device_extensions.data();

    // takes physical device to interface with, the queue and the usage info
    if (m_physical_device.createDevice(&create_info, nullptr, &m_logical_device) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    // since we only set 1 queue we can just index at 0
    m_logical_device.getQueue(indices.graphics_family.value(), 0, &m_graphics_queue);
    m_logical_device.getQueue(indices.present_family.value(), 0, &m_present_queue);

    create_swapchain(extent);
}

Device::~Device()
{
    cleanup_swapchain();

    // devices don't interact directly with instances
    m_logical_device.destroy();
}

void Device::draw_frame(u32 current_frame)
{
}

void Device::create_swapchain(vk::Extent2D extent)
{
    SwapChainSupportDetails swap_chain_support = DeviceHelper::query_swap_chain_support(m_physical_device, *m_surface);

    vk::SurfaceFormatKHR surface_format = DeviceHelper::choose_swap_surface_format(swap_chain_support.formats);
    vk::PresentModeKHR present_mode = DeviceHelper::choose_swap_present_mode(swap_chain_support.present_modes);
    vk::Extent2D actual_extent = DeviceHelper::choose_swap_extent(swap_chain_support.capabilities, extent);

    // it is usually recommended to have 1 extra than the min to avoid GPU hangs
    unsigned image_count = swap_chain_support.capabilities.minImageCount + 1;

    if((swap_chain_support.capabilities.maxImageCount > 0) && (image_count > swap_chain_support.capabilities.maxImageCount))
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info{};
    create_info.sType = vk::StructureType::eSwapchainCreateInfoKHR; // VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = *m_surface;

    // swap chain image details
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = actual_extent;
    create_info.imageArrayLayers = 1; // amount of layers each image consists of
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = DeviceHelper::find_queue_families(m_physical_device, *m_surface);
    unsigned queue_family_indices[] = { indices.graphics_family.value(), indices.present_family.value() };

    if(indices.graphics_family != indices.present_family)
    {
        // images can be used across multiple queue families without explicit ownership transfers
        // requires to state which queue families will be shared
        create_info.imageSharingMode = vk::SharingMode::eConcurrent; // VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        // image owned by one queue family
        // ownership must be explicitly transferred before using it in another one
        create_info.imageSharingMode = vk::SharingMode::eExclusive; // VK_SHARING_MODE_EXCLUSIVE;

        // optional
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    // can set up some kind of transform you want to apply to the images
    // set it to current transform to prevent that
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;

    // specifies if alpha field should be used for blending
    // almost always want to ignore the alpha channel
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE; // don't care about the colour of the pixels being obscured by another window

    // if the swap chain becomes invalid during runtime (ex. window resize)
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if(m_logical_device.createSwapchainKHR(&create_info, nullptr, &m_swapchain) != vk::Result::eSuccess)
    {
        throw std::runtime_error("unable to create swapchain!");
    }

    // we only specified a minimum amount of images to create, the implementation is capable of making more
    vk::Result result = m_logical_device.getSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    m_swapchain_images.resize(image_count);

    result = m_logical_device.getSwapchainImagesKHR(m_swapchain, &image_count, m_swapchain_images.data());

    // need these for later
    m_swapchain_image_format = surface_format.format;
    m_swapchain_extent = actual_extent;
}

void Device::cleanup_swapchain()
{
    for(auto framebuffer : m_swapchain_framebuffers)
    {
        m_logical_device.destroyFramebuffer(framebuffer, nullptr);
    }

    for(auto image_view : m_swapchain_image_views)
    {
        m_logical_device.destroyImageView(image_view, nullptr);
    }

//    m_device.destroyImage(m_depth_image, nullptr);
//    m_device.freeMemory(m_depth_image_memory);
//    m_device.destroyImageView(m_depth_image_view, nullptr);

    m_logical_device.destroySwapchainKHR(m_swapchain, nullptr);
}



