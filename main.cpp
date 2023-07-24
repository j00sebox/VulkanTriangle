#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.hpp>

#define GLM_FORCE_RADIANS
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <map>
#include <optional>
#include <set>
#include <limits>
#include <fstream>
#include <chrono>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Vertex.hpp"

// interleaving vertex attributes
const std::vector<Vertex> g_vertices =
{
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}
};

const std::vector<unsigned> g_indices =
{
        0, 1, 2, 2, 3, 0
};

// reads all bytes from SPIR-V bin and return byte array
static std::vector<char> read_file(const std::string& file_name)
{
    // ate: start reading at the end of the file
    // binary: read file as a binary to avoid text transformations
    std::ifstream file(file_name, std::ios::ate | std::ios::binary);

    if(!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);

    // go back to beginning of file and read it
    file.seekg(0);
    file.read(buffer.data(), file_size);

    file.close();

    return buffer;
}

// function is not readily available so need to go fetch it
vk::DebugUtilsMessengerEXT create_debug_utils_messenger_ext(vk::Instance instance, const vk::DebugUtilsMessengerCreateInfoEXT create_info, vk::DispatchLoaderDynamic& dispatch_loader)
{
    // returns nullptr if function could not be loaded
    return instance.createDebugUtilsMessengerEXT(create_info, nullptr, dispatch_loader);
}

static void framebuffer_resize_callback(GLFWwindow* window, int width, int height);

// STEPS TO DRAW A TRIANGLE
// 1. Create a vk::Instance
// 2. Select supported graphics card (vk::PhysicalDevice)
// 3. Create a vk::Device and vk::Queue for drawing/presenting
// 4. Create window, window surface adn swapchain
// 5. Wrap swapchain images into vk::ImageView objects
// 6. Create renderpass that specifies render targets and usages
// 7. Set up graphics pipeline
// 8. Allocate and record a command buffer for every swapchain image
// 9. Draw frames by acquiring images, submitting the right draw command and returning the
//    images back to the swapchain

class HelloTriangle
{
public:
    void run()
    {
        init_window();
        init_vulkan();
        main_loop();
        cleanup();
    }

    // many drivers trigger the out of date result, it's not guaranteed
    bool m_framebuffer_resized = false;

private:
    GLFWwindow *m_window;
    vk::Instance m_instance;
    vk::DebugUtilsMessengerEXT m_debug_messenger;
    vk::DispatchLoaderDynamic m_dispatch_loader;
    vk::SurfaceKHR m_surface;
    vk::PhysicalDevice m_physical_device = nullptr;
    vk::Device m_device;
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

    vk::Buffer m_vertex_buffer;
    vk::DeviceMemory m_vertex_buffer_memory;
    vk::Buffer m_index_buffer;
    vk::DeviceMemory m_index_buffer_memory;

    // we could set up the shader to access pixel values in a buffer but it is better to use an image
    // with image objects you can retrieve colour data using 2D coordinates
    // pixels within an image are known as texels
    vk::Image m_texture_image;
    vk::DeviceMemory m_texture_image_memory;

    // uniform buffers
    std::vector<vk::Buffer> m_uniform_buffers;
    std::vector<vk::DeviceMemory> m_uniform_buffers_memory;
    std::vector<void*> m_uniform_buffers_mapped;

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

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    // allow multiple frames to be in-flight
    // this means we allow a new frame to start being rendered without interfering with one being presented
    // meaning we need multiple command buffers, semaphores and fences
    const int MAX_FRAMES_IN_FLIGHT = 2;

    // validation layer stuff
    const std::vector<const char *> m_validation_layers =
    {
            "VK_LAYER_KHRONOS_validation" // default one with SDK
    };

    const std::vector<const char *> m_device_extensions =
    {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

#ifdef NDEBUG
    const bool m_enable_validation_layers = false;
#else
    const bool m_enable_validation_layers = true;
#endif

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

    // a descriptor layout specifies the types of resources that are going to be accessed by the pipeline
    // a descriptor set specifies the actual buffer or image resources that will be bound to the descriptors
    struct UniformBufferObject
    {
        glm::mat4  model;
        glm::mat4 view;
        glm::mat4 proj;
    };

    void init_window()
    {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        m_window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

        // GLFW can't create a callback with a member function so we have to make it outside
        // but we can store an arbitrary pointer inside of the window
        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebuffer_resize_callback);
    }

    void init_vulkan()
    {
        create_instance();
        setup_debug_messenger();
        create_surface();
        pick_physical_device();
        create_logical_device();
        create_swapchain();
        create_image_views();
        create_render_pass();
        create_descriptor_set_layout();
        create_graphics_pipeline();
        create_framebuffers();
        create_command_pool();
        create_texture_image();
        create_vertex_buffer();
        create_index_buffer();
        create_uniform_buffers();
        create_descriptor_pool();
        create_descriptor_sets();
        create_command_buffer();
        create_sync_objects();
    }

    void create_instance()
    {
        if (m_enable_validation_layers && !check_validation_layer_support())
        {
            throw std::runtime_error("validation layers requested, but not available");
        }

        // a lot of info is passed through structs
        vk::ApplicationInfo app_info{};
        app_info.sType = vk::StructureType::eApplicationInfo; // VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Hello Triangle";
        app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.pEngineName = "No Engine";
        app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        app_info.apiVersion = VK_API_VERSION_1_1;

        vk::InstanceCreateInfo create_info{};
        create_info.sType =  vk::StructureType::eInstanceCreateInfo; // VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.pApplicationInfo = &app_info;

        // can use glfw to wrangle what extensions are available to us
        auto extensions = get_required_extensions();

        create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        create_info.ppEnabledExtensionNames = extensions.data();

        vk::DebugUtilsMessengerCreateInfoEXT debug_create_info{};
        if (m_enable_validation_layers)
        {
            create_info.enabledLayerCount = static_cast<uint32_t>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();

            // allows debugging on instance creation and destruction
            populate_debug_messenger_create_info(debug_create_info);
            create_info.pNext = &debug_create_info;
        }
        else
        {
            create_info.enabledLayerCount = 0;
        }

        if (vk::createInstance(&create_info, nullptr, &m_instance) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create instance!");
        }

        m_dispatch_loader = vk::DispatchLoaderDynamic(m_instance, vkGetInstanceProcAddr);
    }

    void setup_debug_messenger()
    {
        if (!m_enable_validation_layers) return;

        vk::DebugUtilsMessengerCreateInfoEXT create_info{};
        populate_debug_messenger_create_info(create_info);

        m_debug_messenger = create_debug_utils_messenger_ext(m_instance, create_info, m_dispatch_loader);
    }

    // since Vulkan is platform-agnostic it cannot interact directly with the window system,
    // so we create an object to interface between them
    void create_surface()
    {
        VkSurfaceKHR c_style_surface;
        if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &c_style_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }

        m_surface = c_style_surface;
    }

    void pick_physical_device()
    {
        unsigned device_count = 0;
        vk::Result result = m_instance.enumeratePhysicalDevices(&device_count, nullptr);

        if (device_count == 0)
        {
            throw std::runtime_error("failed to find GPUs with Vulkan support!");
        }

        std::vector<vk::PhysicalDevice> devices(device_count);

        // keeps track of devices and their score to pick the best option
        std::multimap<int, vk::PhysicalDevice> candidates;
        result = m_instance.enumeratePhysicalDevices(&device_count, devices.data());

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

    void create_logical_device()
    {
        QueueFamilyIndices indices = find_queue_families(m_physical_device);

        std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
        std::set<unsigned> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value()};

        float queue_priority = 1.f;
        for (unsigned queue_family: unique_queue_families) {
            vk::DeviceQueueCreateInfo queue_create_info{};

            // described number of queues we want in queue family
            queue_create_info.sType = vk::StructureType::eDeviceQueueCreateInfo; // VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
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

        vk::PhysicalDeviceFeatures device_features{};

        vk::DeviceCreateInfo create_info{};
        create_info.sType = vk::StructureType::eDeviceCreateInfo; // VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        create_info.queueCreateInfoCount = static_cast<unsigned>(queue_create_infos.size());
        create_info.pQueueCreateInfos = queue_create_infos.data();
        create_info.pEnabledFeatures = &device_features;

        // previous versions of Vulkan had a distinction between instance and device specific validation layers
        // meaning enabledLayerCount and ppEnabledLayerNames field are ignored
        // it is still a good idea to set them to be compatible with older versions
        create_info.enabledExtensionCount = static_cast<unsigned>(m_device_extensions.size());
        create_info.ppEnabledExtensionNames = m_device_extensions.data();

        if (m_enable_validation_layers)
        {
            create_info.enabledLayerCount = static_cast<unsigned>(m_validation_layers.size());
            create_info.ppEnabledLayerNames = m_validation_layers.data();
        }
        else
        {
            create_info.enabledLayerCount = 0;
        }

        // takes physical device to interface with, the queue and the usage info
        if (m_physical_device.createDevice(&create_info, nullptr, &m_device) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create logical device!");
        }

        // since we only set 1 queue we can just index at 0
        m_device.getQueue(indices.graphics_family.value(), 0, &m_graphics_queue);
        m_device.getQueue(indices.present_family.value(), 0, &m_present_queue);
    }

    // there is no default framebuffer in Vulkan, so it needs something to own the framebuffers we render to
    // before presenting them onscreen
    // swapchain is essentially a queue of images that are waiting to be presented on screen
    void create_swapchain()
    {
        SwapChainSupportDetails swap_chain_support = query_swap_chain_support(m_physical_device);

        vk::SurfaceFormatKHR surface_format = choose_swap_surface_format(swap_chain_support.formats);
        vk::PresentModeKHR present_mode = choose_swap_present_mode(swap_chain_support.present_modes);
        vk::Extent2D extent = choose_swap_extent(swap_chain_support.capabilities);

        // it is usually recommended to have 1 extra than the min to avoid GPU hangs
        unsigned image_count = swap_chain_support.capabilities.minImageCount + 1;

        if((swap_chain_support.capabilities.maxImageCount > 0) && (image_count > swap_chain_support.capabilities.maxImageCount))
        {
            image_count = swap_chain_support.capabilities.maxImageCount;
        }

        vk::SwapchainCreateInfoKHR create_info{};
        create_info.sType = vk::StructureType::eSwapchainCreateInfoKHR; // VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        create_info.surface = m_surface;

        // swap chain image details
        create_info.minImageCount = image_count;
        create_info.imageFormat = surface_format.format;
        create_info.imageColorSpace = surface_format.colorSpace;
        create_info.imageExtent = extent;
        create_info.imageArrayLayers = 1; // amount of layers each image consists of
        create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment; // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        QueueFamilyIndices indices = find_queue_families(m_physical_device);
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

        // can setup some kind of transform you want to apply to the images
        // set it to current transform to prevent that
        create_info.preTransform = swap_chain_support.capabilities.currentTransform;

        // specifies if alpha field should be used for blending
        // almost always want to ignore the alpha channel
        create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        create_info.presentMode = present_mode;
        create_info.clipped = VK_TRUE; // don't care about the colour of the pixels being obscured by another window

        // if the swap chain becomes invalid during runtime (ex. window resize)
        create_info.oldSwapchain = VK_NULL_HANDLE;

        if(m_device.createSwapchainKHR(&create_info, nullptr, &m_swapchain) != vk::Result::eSuccess)
        {
            throw std::runtime_error("unable to create swapchain!");
        }

        // we only specified a minimum amount of images to create, the implementation is capable of making more
        vk::Result result = m_device.getSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
        m_swapchain_images.resize(image_count);

        result = m_device.getSwapchainImagesKHR(m_swapchain, &image_count, m_swapchain_images.data());

        // need these for later
        m_swapchain_image_format = surface_format.format;
        m_swapchain_extent = extent;
    }

    void create_image_views()
    {
        m_swapchain_image_views.resize(m_swapchain_images.size());

        for(size_t i = 0; i < m_swapchain_images.size(); ++i)
        {
            vk::ImageViewCreateInfo create_info{};
            create_info.sType = vk::StructureType::eImageViewCreateInfo; // VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = m_swapchain_images[i];

            // how image data should be interpreted
            // 1D textures, 2D textures, 3D textures and cube maps
            create_info.viewType = vk::ImageViewType::e2D; // VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = m_swapchain_image_format;

            // allows swizzling of the colour channels
            // sticking to default mapping
            create_info.components.r = vk::ComponentSwizzle::eIdentity; // VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = vk::ComponentSwizzle::eIdentity; // VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = vk::ComponentSwizzle::eIdentity; // VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = vk::ComponentSwizzle::eIdentity; // VK_COMPONENT_SWIZZLE_IDENTITY;

            // describes what the image's purpose is and which part of the image should be used
            // these images will be used as colour targets without any mipmapping
            create_info.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor; // VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            if(m_device.createImageView(&create_info, nullptr, &m_swapchain_image_views[i]) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create image view!");
            }
        }
    }

    // need to tell Vulkan about framebuffer attachments
    void create_render_pass()
    {
        // in this case we just have a single colour buffer
        vk::AttachmentDescription colour_attachment{};
        colour_attachment.format = m_swapchain_image_format;
        colour_attachment.samples = vk::SampleCountFlagBits::e1; // VK_SAMPLE_COUNT_1_BIT;

        // these determine what to do with the data in the attachment before and after rendering
        // applies to both colour and depth
        colour_attachment.loadOp = vk::AttachmentLoadOp::eClear; // VK_ATTACHMENT_LOAD_OP_CLEAR; // clear at start
        colour_attachment.storeOp = vk::AttachmentStoreOp::eStore; // VK_ATTACHMENT_STORE_OP_STORE; // rendered pixels wil be stored

        // not using stencil now
        colour_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare; // VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colour_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare; // VK_ATTACHMENT_STORE_OP_DONT_CARE;

        // textures are represented by VKImage objects with a certain pixel format
        // initial layout specifies what layout the image will have before the render pass
        // final layout is what to transition to after the render pass
        // we set the initial layout as undefined because we don't care what the previous layout was before
        // we want the image to be ready for presentation using the swap chain hence the final layout
        colour_attachment.initialLayout = vk::ImageLayout::eUndefined; // VK_IMAGE_LAYOUT_UNDEFINED;
        colour_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR; // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        // a render pass can have multiple subpasses
        // they are subsequent passes that depend on the contents of framebuffers in previous passes
        // every subpass references one or more previously described attachments
        vk::AttachmentReference colour_attachment_ref{};
        colour_attachment_ref.attachment = 0; // references attachment by index
        colour_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal; // VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL; // layout attachment should have during subpass

        vk::SubpassDescription subpass{};
        subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics; // VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colour_attachment_ref;

        // subpasses in a render pass automatically take care of image layout transitions
        // these transitions are controlled by subpass dependencies
        // since we only have 1 subpass, the operations before and after are implicit subpasses
        vk::SubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to implicit subpass before
        dependency.dstSubpass = 0; // index to our subpass

        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = vk::AccessFlagBits::eNone;
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput; // VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite; // VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        vk::RenderPassCreateInfo render_pass_info{};
        render_pass_info.sType = vk::StructureType::eRenderPassCreateInfo; // VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        render_pass_info.attachmentCount = 1;
        render_pass_info.pAttachments = &colour_attachment;
        render_pass_info.subpassCount = 1;
        render_pass_info.pSubpasses = &subpass;
        render_pass_info.dependencyCount = 1;
        render_pass_info.pDependencies = &dependency;

        if(m_device.createRenderPass(&render_pass_info, nullptr, &m_render_pass) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void create_descriptor_set_layout()
    {
        vk::DescriptorSetLayoutBinding ubo_layout_binding{};
        ubo_layout_binding.binding = 0; // binding in the shader
        ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
        ubo_layout_binding.descriptorCount = 1;
        ubo_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex;
        ubo_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling descriptors

        vk::DescriptorSetLayoutCreateInfo layout_info{};
        layout_info.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
        layout_info.bindingCount = 1;
        layout_info.pBindings = &ubo_layout_binding;

        if(m_device.createDescriptorSetLayout(&layout_info, nullptr, &m_descriptor_set_layout) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    void create_graphics_pipeline()
    {
        auto vert_shader_code = read_file("shaders/vert.spv");
        auto frag_shader_code = read_file("shaders/frag.spv");

        vk::ShaderModule vert_shader_module = create_shader_module(vert_shader_code);
        vk::ShaderModule frag_shader_module = create_shader_module(frag_shader_code);

        // to use these shaders we need to assign them to a specific pipeline stage
        vk::PipelineShaderStageCreateInfo vert_shader_stage_info{};
        vert_shader_stage_info.sType = vk::StructureType::ePipelineShaderStageCreateInfo; // VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex; //VK_SHADER_STAGE_VERTEX_BIT;

        vert_shader_stage_info.module = vert_shader_module;
        vert_shader_stage_info.pName = "main"; // entrypoint

        vk::PipelineShaderStageCreateInfo frag_shader_stage_info{};
        frag_shader_stage_info.sType = vk::StructureType::ePipelineShaderStageCreateInfo;

        // define what stage it belongs to
        frag_shader_stage_info.stage = vk::ShaderStageFlagBits::eFragment;

        frag_shader_stage_info.module = frag_shader_module;
        frag_shader_stage_info.pName = "main"; // entrypoint

        vk::PipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

        // while most of the pipeline state needs to be baked into the pipeline state
        // a limited amount of the state can actually be changed without recreating
        // ex. size of viewport, line width, blend constants
        // to use dynamic state you need to fill out the struct
        std::vector<vk::DynamicState> dynamic_states =
        {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };

        // the configuration of these values will be ignored, so they can be changed at runtime
        vk::PipelineDynamicStateCreateInfo dynamic_state{};
        dynamic_state.sType = vk::StructureType::ePipelineDynamicStateCreateInfo;
        dynamic_state.dynamicStateCount = static_cast<unsigned>(dynamic_states.size());
        dynamic_state.pDynamicStates = dynamic_states.data();

        auto binding_description = Vertex::get_binding_description();
        auto attribute_descriptions = Vertex::get_attribute_description();

        // need to describe the format of the vertex data
        // bindings: spacing between data and whether it is per vertex or instance
        // attribute descriptions: type of the attributes passed to the vertex shader
        vk::PipelineVertexInputStateCreateInfo vertex_input_info{};
        vertex_input_info.sType = vk::StructureType::ePipelineVertexInputStateCreateInfo;
        vertex_input_info.vertexBindingDescriptionCount = 1;
        vertex_input_info.pVertexBindingDescriptions = &binding_description;
        vertex_input_info.vertexAttributeDescriptionCount = static_cast<unsigned>(attribute_descriptions.size());
        vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.data();

        // input assembly describes what kind of geometry we are passing
        vk::PipelineInputAssemblyStateCreateInfo input_assembly{};
        input_assembly.sType = vk::StructureType::ePipelineInputAssemblyStateCreateInfo;
        input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
        input_assembly.primitiveRestartEnable = false; // idk

        // viewport is the region of the framebuffer that the output will be rendered to
        // (0,0) to (width, height)
        // the swapchain extent may be different than the window size
        vk::Viewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = (float)m_swapchain_extent.width;
        viewport.height = (float)m_swapchain_extent.height;

        // range of depth values to use in framebuffer
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        // scissor rectangles define which regions pixels will be stored
        // any pixel outside the scissor rectangle will be discarded
        // to draw entire framebuffer we just make it the same size
        vk::Rect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = m_swapchain_extent;

        // since we made viewport and scissor dynamic we don't need to bind them here
        vk::PipelineViewportStateCreateInfo viewport_state{};
        viewport_state.sType = vk::StructureType::ePipelineViewportStateCreateInfo;
        viewport_state.viewportCount = 1;
        viewport_state.scissorCount = 1;

        vk::PipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = vk::StructureType::ePipelineRasterizationStateCreateInfo;
        rasterizer.depthClampEnable = false; // if true then fragments beyond near and far planes will be clamped
        rasterizer.rasterizerDiscardEnable = false; // if true then geometry never goes to rasterizer stage

        // determines how fragments are generated for geometry
        // fill means the triangles are completely coloured in
        rasterizer.polygonMode = vk::PolygonMode::eFill;

        // line width described the thickness of lines in number of fragments
        // the maximum line width that is supported depends on hardware
        rasterizer.lineWidth = 1.f;

        rasterizer.cullMode = vk::CullModeFlagBits::eBack;
        rasterizer.frontFace = vk::FrontFace::eCounterClockwise;

        rasterizer.depthBiasEnable = false;
        rasterizer.depthBiasConstantFactor = 0.f;
        rasterizer.depthBiasClamp = 0.f;
        rasterizer.depthBiasSlopeFactor = 0.f;

        vk::PipelineMultisampleStateCreateInfo multi_sampling{};
        multi_sampling.sType = vk::StructureType::ePipelineMultisampleStateCreateInfo;
        multi_sampling.sampleShadingEnable = false;
        multi_sampling.rasterizationSamples = vk::SampleCountFlagBits::e1;

//        multi_sampling.minSampleShading = 1.f;
//        multi_sampling.pSampleMask = nullptr;
//        multi_sampling.alphaToCoverageEnable = false;
//        multi_sampling.alphaToOneEnable = false;

        // if using depth or stencil buffer then they need to be configured
        // vk::PipelineDepthStencilStateCreateInfo

        // colour blending
        // this way of blending contains a configuration per attached framebuffer
        vk::PipelineColorBlendAttachmentState colour_blend_attachment{};
        colour_blend_attachment.colorWriteMask = vk::ColorComponentFlagBits::eR
                | vk::ColorComponentFlagBits::eG
                | vk::ColorComponentFlagBits::eB
                | vk::ColorComponentFlagBits::eA;

        // if false then the new colour from the fragment shader is passed unmodified
        colour_blend_attachment.blendEnable = false;
        colour_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
        colour_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eZero;
        colour_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
        colour_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
        colour_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
        colour_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

        vk::PipelineColorBlendStateCreateInfo colour_blending{};
        colour_blending.sType = vk::StructureType::ePipelineColorBlendStateCreateInfo;
        colour_blending.logicOpEnable = false;
        colour_blending.logicOp = vk::LogicOp::eCopy; // optional
        colour_blending.attachmentCount = 1;
        colour_blending.pAttachments = &colour_blend_attachment;

        // optional
        colour_blending.blendConstants[0] = 0.f;
        colour_blending.blendConstants[1] = 0.f;
        colour_blending.blendConstants[2] = 0.f;
        colour_blending.blendConstants[3] = 0.f;


        vk::PipelineLayoutCreateInfo pipeline_layout_info{};
        pipeline_layout_info.sType = vk::StructureType::ePipelineLayoutCreateInfo;

        // need to specify the descriptor set layout here
        pipeline_layout_info.setLayoutCount = 1;
        pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
        pipeline_layout_info.pushConstantRangeCount = 0;
        pipeline_layout_info.pPushConstantRanges = nullptr;

        if(m_device.createPipelineLayout(&pipeline_layout_info, nullptr, &m_pipeline_layout) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        vk::GraphicsPipelineCreateInfo pipeline_info{};
        pipeline_info.sType = vk::StructureType::eGraphicsPipelineCreateInfo;
        pipeline_info.stageCount = 2;
        pipeline_info.pStages = shader_stages;

        // reference all structures in fixed function stage
        pipeline_info.pVertexInputState = &vertex_input_info;
        pipeline_info.pInputAssemblyState = &input_assembly;
        pipeline_info.pViewportState = &viewport_state;
        pipeline_info.pRasterizationState = &rasterizer;
        pipeline_info.pMultisampleState = &multi_sampling;
        pipeline_info.pDepthStencilState = nullptr; // optional
        pipeline_info.pColorBlendState = &colour_blending;
        pipeline_info.pDynamicState = &dynamic_state;

        pipeline_info.layout = m_pipeline_layout;

        // reference the renderpass and index of subpass
        pipeline_info.renderPass = m_render_pass;
        pipeline_info.subpass = 0;

        // Vulkan allows you to create new pipelines derived from existing ones
        // we don't need this now so just set to null
        pipeline_info.basePipelineHandle = nullptr;
        pipeline_info.basePipelineIndex = -1;

        // you can actually use this one function call to make multiple pipelines
        // the second param references a vk::PipelineCache object
        if(m_device.createGraphicsPipelines(nullptr, 1, &pipeline_info, nullptr, &m_graphics_pipeline) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        m_device.destroyShaderModule(vert_shader_module, nullptr);
        m_device.destroyShaderModule(frag_shader_module, nullptr);
    }

    void create_framebuffers()
    {
        m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

        for(size_t i = 0; i < m_swapchain_image_views.size(); ++i)
        {
            vk::ImageView attachments[] = { m_swapchain_image_views[i] };

            // you can only use a framebuffer with a compatible render pass
            // meaning they use the same number and type of attachments
            vk::FramebufferCreateInfo framebuffer_info{};
            framebuffer_info.sType = vk::StructureType::eFramebufferCreateInfo;
            framebuffer_info.renderPass = m_render_pass;
            framebuffer_info.attachmentCount = 1;
            framebuffer_info.pAttachments = attachments;
            framebuffer_info.width = m_swapchain_extent.width;
            framebuffer_info.height = m_swapchain_extent.height;
            framebuffer_info.layers = 1;

            if(m_device.createFramebuffer(&framebuffer_info, nullptr, &m_swapchain_framebuffers[i]) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void create_command_pool()
    {
        QueueFamilyIndices queue_family_indices = find_queue_families(m_physical_device);

        vk::CommandPoolCreateInfo pool_info{};
        pool_info.sType = vk::StructureType::eCommandPoolCreateInfo;

        // allows command buffers to be rerecorded individually
        // without this flag they all have to be reset together
        // we will be recording a command buffer every frame so we want to be able to write to it
        pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

        // command buffers are executed by submitting them on one of the device queues
        // each command pool can only allocate command buffers that will be submitted to the same type of queue
        // since we're recording commands for drawing we make it the graphics queue
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

        if(m_device.createCommandPool(&pool_info, nullptr, &m_command_pool) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    void create_texture_image()
    {
        int width, height, channels;
        stbi_uc* pixels = stbi_load("textures/statue_face.jpg", &width, &height, &channels, STBI_rgb_alpha);

        vk::DeviceSize image_size = width * height * 4;

        if(!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_buffer_memory;

        // should be in host visible memory so that we can map it and should be usable as a transfer source
        create_buffer(image_size, vk::BufferUsageFlagBits::eTransferSrc,
                      vk::MemoryPropertyFlagBits::eHostVisible |
                      vk::MemoryPropertyFlagBits::eHostCoherent,
                      staging_buffer, staging_buffer_memory);

        void* data = m_device.mapMemory(staging_buffer_memory, 0, image_size);
        memcpy(data, pixels, static_cast<size_t>(image_size));
        m_device.unmapMemory(staging_buffer_memory);

        stbi_image_free(pixels);

        create_image(width, height, vk::Format::eR8G8B8A8Srgb, vk::ImageTiling::eOptimal,
                     vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                     vk::MemoryPropertyFlagBits::eDeviceLocal, m_texture_image, m_texture_image_memory);

        transition_image_layout(m_texture_image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        copy_buffer_to_image(staging_buffer, m_texture_image, static_cast<unsigned>(width), static_cast<unsigned>(height));

        transition_image_layout(m_texture_image, vk::Format::eR8G8B8A8Srgb, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

        m_device.destroyBuffer(staging_buffer, nullptr);
        m_device.freeMemory(staging_buffer_memory, nullptr);
    }

    void create_image(unsigned width, unsigned height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage,
                      vk::MemoryPropertyFlags properties, vk::Image& image, vk::DeviceMemory& image_memory)
    {
        vk::ImageCreateInfo image_info{};
        image_info.sType = vk::StructureType::eImageCreateInfo;
        image_info.imageType = vk::ImageType::e2D; // what kind of coordinate system to use
        image_info.extent.width = static_cast<unsigned>(width);
        image_info.extent.height = static_cast<unsigned>(height);
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;

        // use the same format for the texels as the pixels in the buffer
        image_info.format = format; // = vk::_FORMAT_R8G8B8A8_SRGB;

        // defines how the texels are laid out in memory
        image_info.tiling = tiling; // = vk::_IMAGE_TILING_OPTIMAL;

        // since we are transitioning the image to a new place anyway
        image_info.initialLayout = vk::ImageLayout::eUndefined;

        // image will be used as a destination to copy the pixel data to
        // also want to be able to access the image from the shader
        image_info.usage = usage; // vk::_IMAGE_USAGE_TRANSFER_DST_BIT | vk::_IMAGE_USAGE_SAMPLED_BIT;

        // will only be used by one queue family
        image_info.sharingMode = vk::SharingMode::eExclusive;

        // images used as textures don't really need to be multisampled
        image_info.samples = vk::SampleCountFlagBits::e1;

        if(m_device.createImage(&image_info, nullptr, &image) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create image!");
        }

        vk::MemoryRequirements memory_requirements;

        m_device.getImageMemoryRequirements(image, &memory_requirements);

        vk::MemoryAllocateInfo alloc_info{};
        alloc_info.sType = vk::StructureType::eMemoryAllocateInfo;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

        if(m_device.allocateMemory(&alloc_info, nullptr, &image_memory) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to allocate image memory!");
        }

        m_device.bindImageMemory(image, image_memory, 0);
    }

    void create_vertex_buffer()
    {
        vk::DeviceSize buffer_size = sizeof(g_vertices[0]) * g_vertices.size();

        // staging buffer is the temporary buffer on CPU memory that gets transferred to the GPU
        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_buffer_memory;
        create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                      vk::MemoryPropertyFlagBits::eHostVisible |
                      vk::MemoryPropertyFlagBits::eHostCoherent,
                      staging_buffer, staging_buffer_memory);

        //  map the buffer memory into CPU accessible memory
        void* data = m_device.mapMemory(staging_buffer_memory, 0, buffer_size);
        memcpy(data, g_vertices.data(), (size_t)buffer_size);
        m_device.unmapMemory(staging_buffer_memory);

        create_buffer(buffer_size, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                      vk::MemoryPropertyFlagBits::eDeviceLocal,
                      m_vertex_buffer, m_vertex_buffer_memory);

        copy_buffer(staging_buffer, m_vertex_buffer, buffer_size);

        m_device.destroyBuffer(staging_buffer, nullptr);
        m_device.freeMemory(staging_buffer_memory, nullptr);
    }

    void create_index_buffer()
    {
        vk::DeviceSize buffer_size = sizeof(g_indices[0]) * g_indices.size();

        vk::Buffer staging_buffer;
        vk::DeviceMemory staging_buffer_memory;
        create_buffer(buffer_size, vk::BufferUsageFlagBits::eTransferSrc,
                      vk::MemoryPropertyFlagBits::eHostVisible |
                      vk::MemoryPropertyFlagBits::eHostCoherent,
                      staging_buffer, staging_buffer_memory);

        //  map the buffer memory into CPU accessible memory
        void* data = m_device.mapMemory(staging_buffer_memory, 0, buffer_size);
        memcpy(data, g_indices.data(), (size_t)buffer_size);
        m_device.unmapMemory(staging_buffer_memory);

        create_buffer(buffer_size, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                      vk::MemoryPropertyFlagBits::eDeviceLocal,
                      m_index_buffer, m_index_buffer_memory);

        copy_buffer(staging_buffer, m_index_buffer, buffer_size);

        m_device.destroyBuffer(staging_buffer, nullptr);
        m_device.freeMemory(staging_buffer_memory, nullptr);
    }

    void create_uniform_buffers()
    {
        vk::DeviceSize buffer_size = sizeof(UniformBufferObject);

        m_uniform_buffers.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniform_buffers_memory.resize(MAX_FRAMES_IN_FLIGHT);
        m_uniform_buffers_mapped.resize(MAX_FRAMES_IN_FLIGHT);

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            create_buffer(buffer_size, vk::BufferUsageFlagBits::eUniformBuffer,
                          vk::MemoryPropertyFlagBits::eHostVisible |
                          vk::MemoryPropertyFlagBits::eHostCoherent,
                          m_uniform_buffers[i], m_uniform_buffers_memory[i]);

            // we map buffer right after to get a pointer, so we can change the data later
            // the buffer stays mapped to this pointer for the application's whole lifetime
            // this is called persistent mapping, mapping is not free
            m_uniform_buffers_mapped[i] = m_device.mapMemory(m_uniform_buffers_memory[i], 0, buffer_size);
        }
    }

    // descriptor sets cannot be created directly, they must be allocated from a pool
    void create_descriptor_pool()
    {
        // first describe which descriptor types pur descriptor sets use and how many
        // we allocate one of these descriptors for every frame
        vk::DescriptorPoolSize pool_size{};
        pool_size.type = vk::DescriptorType::eUniformBuffer;
        pool_size.descriptorCount = static_cast<unsigned>(MAX_FRAMES_IN_FLIGHT);

        vk::DescriptorPoolCreateInfo pool_info{};
        pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = &pool_size;

        // describe max amount of individual descriptors that may be allocated
        pool_info.maxSets = static_cast<unsigned>(MAX_FRAMES_IN_FLIGHT);\

        if(m_device.createDescriptorPool(&pool_info, nullptr, &m_descriptor_pool) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create descriptor pool!");
        }
    }

    void create_descriptor_sets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, m_descriptor_set_layout);

        vk::DescriptorSetAllocateInfo alloc_info{};
        alloc_info.sType = vk::StructureType::eDescriptorSetAllocateInfo;
        alloc_info.descriptorPool = m_descriptor_pool;
        alloc_info.descriptorSetCount = static_cast<unsigned>(MAX_FRAMES_IN_FLIGHT);
        alloc_info.pSetLayouts = layouts.data();

        // we will create one descriptor set for each frame with the same layout
        m_descriptor_sets.resize(MAX_FRAMES_IN_FLIGHT);

        // this call will allocate descriptor sets, each with one uniform buffer descriptor
        if(m_device.allocateDescriptorSets(&alloc_info, m_descriptor_sets.data()) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to allocate descriptor sets!");
        } // you don't need to clean up descriptor sets as they will be freed when the pool is destroyed

        // the descriptor sets are allocated but still need to be configured
        // descriptors that refer to buffers are configured like this
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            vk::DescriptorBufferInfo buffer_info{};
            buffer_info.buffer = m_uniform_buffers[i];
            buffer_info.offset = 0;
            buffer_info.range = sizeof(UniformBufferObject); // or vk::_WHOLE_SIZE

            // the config is updated using the vk::UpdateDescriptorSets function
            vk::WriteDescriptorSet descriptor_write{};
            descriptor_write.sType = vk::StructureType::eWriteDescriptorSet;
            descriptor_write.dstSet = m_descriptor_sets[i]; // descriptor set to update
            descriptor_write.dstBinding = 0; // index binding 0
            descriptor_write.dstArrayElement = 0;

            descriptor_write.descriptorType = vk::DescriptorType::eUniformBuffer;
            descriptor_write.descriptorCount = 1; // how many array elements to update

            descriptor_write.pBufferInfo = &buffer_info;

            // used for descriptors that reference image data
            descriptor_write.pImageInfo = nullptr;
            descriptor_write.pTexelBufferView = nullptr;

            m_device.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
        }
    }

    // memory transfer ops are executed with command buffers
    void copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size)
    {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::BufferCopy copy_region{};
        copy_region.srcOffset = 0; // optional
        copy_region.dstOffset = 0; // optional
        copy_region.size = size;
        command_buffer.copyBuffer(src_buffer, dst_buffer, 1, &copy_region);

        end_single_time_commands(command_buffer);
    }

    void copy_buffer_to_image(vk::Buffer buffer, vk::Image image, unsigned width, unsigned height)
    {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = vk::Offset3D{0,0,0};
        region.imageExtent = vk::Extent3D
        {
            width,
            height,
            1
        };

        command_buffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, 1, &region);

        end_single_time_commands(command_buffer);
    }

    void create_buffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags  properties,
                       vk::Buffer& buffer, vk::DeviceMemory& buffer_memory)
    {
        vk::BufferCreateInfo buffer_info{};
        buffer_info.sType = vk::StructureType::eBufferCreateInfo;
        buffer_info.size = size;
        buffer_info.usage = usage;
        buffer_info.sharingMode = vk::SharingMode::eExclusive;

        if(m_device.createBuffer(&buffer_info, nullptr, &buffer) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        // for some reason to still need to allocate memory for this buffer
        // this struct contains:
        // - size: size of the required amount of memory in bytes, may differ from buffer_info.size
        // - alignment: the offset where the buffer begins in allocated region of memory
        // - memoryTypeBits: Bit field of the memory types that are suitable for the buffer
        vk::MemoryRequirements memory_requirements;
        m_device.getBufferMemoryRequirements(buffer, &memory_requirements);

        // in a real world application you're not supposed to call vk::AllocateMemory for every buffer
        // the max number of simultaneous memory allocations is limited by the maxMemoryAllocationCount physical device limit
        // this can be as low as 4096
        // you want to use a custom allocator like VMA that splits up a single allocation among many different objects
        vk::MemoryAllocateInfo alloc_info{};
        alloc_info.sType = vk::StructureType::eMemoryAllocateInfo;
        alloc_info.allocationSize = memory_requirements.size;
        alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

        if(m_device.allocateMemory(&alloc_info, nullptr, &buffer_memory) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        m_device.bindBufferMemory(buffer, buffer_memory, 0);
    }

    // graphics card can offer different type of memory to allocate from,
    // so we need to choose the best one that suits our needs
    unsigned find_memory_type(unsigned type_filter, vk::MemoryPropertyFlags properties)
    {
        // has two arrays, memoryTpyes and memoryHeaps
        // the heaps are distinct memory sources like VRAM and swap space in RAM
        vk::PhysicalDeviceMemoryProperties memory_properties;
        m_physical_device.getMemoryProperties(&memory_properties);

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
    }

    void create_command_buffer()
    {
        m_command_buffers.resize(MAX_FRAMES_IN_FLIGHT);

        vk::CommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
        alloc_info.commandPool = m_command_pool;

        // primary buffers can be submitted to a queue for execution but cannot be called from other command buffers
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandBufferCount = static_cast<unsigned>(m_command_buffers.size());

        if(m_device.allocateCommandBuffers(&alloc_info, m_command_buffers.data()) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to allocate command buffers!");
        }
    }

    void create_sync_objects()
    {
        m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_render_finished_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
        m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

        vk::SemaphoreCreateInfo semaphore_info{};
        semaphore_info.sType = vk::StructureType::eSemaphoreCreateInfo;

        vk::FenceCreateInfo fence_info{};
        fence_info.sType = vk::StructureType::eFenceCreateInfo;

        // need it to start as signaled so wait for fence will return immediately on the first frame
        fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if(m_device.createSemaphore(&semaphore_info, nullptr, &m_image_available_semaphores[i]) != vk::Result::eSuccess ||
               m_device.createSemaphore(&semaphore_info, nullptr, &m_render_finished_semaphores[i]) != vk::Result::eSuccess ||
               m_device.createFence(&fence_info, nullptr, &m_in_flight_fences[i]) != vk::Result::eSuccess
            )
            {
                throw std::runtime_error("failed to create sync objects!");
            }
        }
    }

    void main_loop()
    {
        while (!glfwWindowShouldClose(m_window))
        {
            glfwPollEvents();
            draw_frame();
        }

        // this prevents the program from ending mid-frame
       m_device.waitIdle();
    }

    // keeps track of the current frame index
    unsigned m_current_frame = 0;

    // wait for previous frame to finish
    // acquire image from swapchain
    // record a command buffer which draws the scene onto the image
    // submit that command buffer
    // present swapchain image
    void draw_frame()
    {
        // takes array of fences and waits for any and all fences
        // saying VK_TRUE means we want to wait for all fences
        // last param is the timeout which we basically disable
        vk::Result result = m_device.waitForFences(1, &m_in_flight_fences[m_current_frame], true, UINT64_MAX);

        unsigned image_index;
        // logical device and swapchain we want to get the image from
        // third param is timeout for image to become available
        // next 2 params are sync objects to be signaled when presentation engine is done with the image
        result = m_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_image_available_semaphores[m_current_frame], nullptr, &image_index);

        // if swapchain is not good we immediately recreate and try again in the next frame
        if(result == vk::Result::eErrorOutOfDateKHR)
        {
            recreate_swapchain();
            return;
        }

        // need to reset fences to unsignaled
        // but only reset if we are submitting work
        result = m_device.resetFences(1, &m_in_flight_fences[m_current_frame]);

        update_uniform_buffer(m_current_frame);

        m_command_buffers[m_current_frame].reset();
        record_command_buffer(m_command_buffers[m_current_frame], image_index);

        vk::SubmitInfo submit_info{};
        submit_info.sType = vk::StructureType::eSubmitInfo;

        // we are specifying what semaphores we want to use and what stage we want to wait on
        vk::Semaphore wait_semaphores[] = {m_image_available_semaphores[m_current_frame]};
        vk::PipelineStageFlags wait_stages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores = wait_semaphores;
        submit_info.pWaitDstStageMask = wait_stages;

        // which command buffers to submit
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &m_command_buffers[m_current_frame];

        // which semaphores to signal once the command buffer is finished
        vk::Semaphore signal_semaphores[] = {m_render_finished_semaphores[m_current_frame]};
        submit_info.signalSemaphoreCount = 1;
        submit_info.pSignalSemaphores = signal_semaphores;

        if(m_graphics_queue.submit(1, &submit_info, m_in_flight_fences[m_current_frame]) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to submit draw command!");
        }

        // last step is to submit the result back to the swapchain
        vk::PresentInfoKHR present_info{};
        present_info.sType = vk::StructureType::ePresentInfoKHR;
        present_info.waitSemaphoreCount = 1;
        present_info.pWaitSemaphores = signal_semaphores;

        vk::SwapchainKHR swapchains[] = {m_swapchain};
        present_info.swapchainCount = 1;
        present_info.pSwapchains = swapchains;
        present_info.pImageIndices = &image_index;

        // one last optional param
        // can get an array of vk::Result to check every swapchain to see if presentation was successful
        present_info.pResults = nullptr;

        result = m_present_queue.presentKHR(&present_info);

        if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || m_framebuffer_resized)
        {
            m_framebuffer_resized = false;
            recreate_swapchain();
        }

        m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void update_uniform_buffer(unsigned current_image)
    {
        static auto start_time = std::chrono::high_resolution_clock::now();

        auto current_time = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(current_time - start_time).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), m_swapchain_extent.width / (float) m_swapchain_extent.height, 0.1f, 10.0f);

        // GLM was originally designed for OpenGL where the Y coordinate of the clip space is inverted,
        // so we can remedy this by flipping the sign of the Y scaling factor in the projection matrix
        ubo.proj[1][1] *= -1;

        // this is the most efficient way to pass constantly changing values to the shader
        // another way to handle smaller data is to use push constants
        memcpy(m_uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
    }

    void cleanup_swapchain()
    {
        for(auto framebuffer : m_swapchain_framebuffers)
        {
            m_device.destroyFramebuffer(framebuffer, nullptr);
        }

        for(auto image_view : m_swapchain_image_views)
        {
            m_device.destroyImageView(image_view, nullptr);
        }

        m_device.destroySwapchainKHR(m_swapchain, nullptr);
    }

    // it's possible for the window surface to change such that the swapchain is no longer compatible
    // one reason is changing the window size
    // we don't recreate the renderpass here because we probably haven't changed the image format
    void recreate_swapchain()
    {
        // handle the case where window is minimized
        int width = 0, height = 0;
        glfwGetFramebufferSize(m_window, &width, &height);

        // essentially pausing the application until the window comes back
        while(width == 0 || height == 0)
        {
            glfwGetFramebufferSize(m_window, &width, &height);
            glfwWaitEvents();
        }

        // want to avoid touching resources that still may be in use
        m_device.waitIdle();

        cleanup_swapchain();

        create_swapchain();
        create_image_views();
        create_framebuffers();
    }

    void cleanup()
    {
        for(size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            // sync objects
            m_device.destroySemaphore(m_image_available_semaphores[i], nullptr);
            m_device.destroySemaphore(m_render_finished_semaphores[i], nullptr);
            m_device.destroyFence(m_in_flight_fences[i], nullptr);

            // uniform buffers
            m_device.destroyBuffer(m_uniform_buffers[i], nullptr);
            m_device.freeMemory(m_uniform_buffers_memory[i], nullptr);
        }

        cleanup_swapchain();

        m_device.destroyImage(m_texture_image, nullptr);
        m_device.freeMemory(m_texture_image_memory, nullptr);

        m_device.destroyDescriptorPool(m_descriptor_pool, nullptr);
        m_device.destroyDescriptorSetLayout(m_descriptor_set_layout, nullptr);

        m_device.destroyBuffer(m_index_buffer, nullptr);
        m_device.freeMemory(m_index_buffer_memory, nullptr);

        m_device.destroyBuffer(m_vertex_buffer, nullptr);
        m_device.freeMemory(m_vertex_buffer_memory, nullptr);

        m_device.destroyCommandPool(m_command_pool, nullptr);
        m_device.destroyPipeline(m_graphics_pipeline, nullptr);
        m_device.destroyPipelineLayout(m_pipeline_layout, nullptr);
        m_device.destroyRenderPass(m_render_pass, nullptr);

        // devices don't interact directly with instances
        m_device.destroy();
        if (m_enable_validation_layers)
        {
            m_instance.destroyDebugUtilsMessengerEXT(m_debug_messenger, nullptr, m_dispatch_loader); // destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
        }
        m_instance.destroySurfaceKHR(m_surface, nullptr);
        m_instance.destroy();
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    vk::CommandBuffer begin_single_time_commands()
    {
        vk::CommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
        alloc_info.level = vk::CommandBufferLevel::ePrimary;
        alloc_info.commandPool = m_command_pool;
        alloc_info.commandBufferCount = 1;

        vk::CommandBuffer command_buffer;
        vk::Result res = m_device.allocateCommandBuffers(&alloc_info, &command_buffer);

        // only using the command buffer once and wait with until the copy is finished
        // its good practice to tell the driver about intend, hence one time submit flag
        vk::CommandBufferBeginInfo begin_info{};
        begin_info.sType = vk::StructureType::eCommandBufferBeginInfo;
        begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

        res = command_buffer.begin(&begin_info);

        return command_buffer;
    }

    void end_single_time_commands(vk::CommandBuffer command_buffer)
    {
        command_buffer.end();

        vk::SubmitInfo  submit_info{};
        submit_info.sType = vk::StructureType::eSubmitInfo;
        submit_info.commandBufferCount = 1;
        submit_info.pCommandBuffers = &command_buffer;

        vk::Result res = m_graphics_queue.submit(1, &submit_info, nullptr);
        m_graphics_queue.waitIdle();

        m_device.freeCommandBuffers(m_command_pool, 1, & command_buffer);
    }

    // if the command buffer has already been recorded then this will reset it,
    // it's not possible to append commands at a later time
    void record_command_buffer(vk::CommandBuffer& command_buffer, unsigned image_index)
    {
        vk::CommandBufferBeginInfo begin_info{};
        begin_info.sType = vk::StructureType::eCommandBufferBeginInfo;
        begin_info.pInheritanceInfo = nullptr; // only relevant to secondary command buffers

        if(command_buffer.begin(&begin_info) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to begin recording of framebuffer!");
        }

        vk::RenderPassBeginInfo renderpass_info{};
        renderpass_info.sType = vk::StructureType::eRenderPassBeginInfo;
        renderpass_info.renderPass = m_render_pass;

        // need to bind the framebuffer for the swapchain image we want to draw to
        renderpass_info.framebuffer = m_swapchain_framebuffers[image_index];

        // define size of the render area;
        // render area defined as the place shader loads and stores happen
        renderpass_info.renderArea.offset = vk::Offset2D{0, 0};
        renderpass_info.renderArea.extent = m_swapchain_extent;

        vk::ClearValue clear_colour = {{0.f, 0.f, 0.f, 1.f}};
        renderpass_info.clearValueCount = 1;
        renderpass_info.pClearValues = &clear_colour;

        // all functions that record commands cna be recognized by their vk::Cmd prefix
        // they all return void, so no error handling until the recording is finished
        // we use inline since this is a primary command buffer
        command_buffer.beginRenderPass(&renderpass_info, vk::SubpassContents::eInline);

        // second param specifies if the object is graphics or compute
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, m_graphics_pipeline);

        vk::Buffer vertex_buffers[] = {m_vertex_buffer};
        vk::DeviceSize offsets[] = {0};
        command_buffer.bindVertexBuffers(0, 1, vertex_buffers, offsets);

        command_buffer.bindIndexBuffer(m_index_buffer, 0, vk::IndexType::eUint32);

        // since we specified that the viewport and scissor were dynamic we need to do them now
        vk::Viewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(m_swapchain_extent.width);
        viewport.height = static_cast<float>(m_swapchain_extent.height);
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        command_buffer.setViewport(0, 1, &viewport);

        vk::Rect2D scissor{};
        scissor.offset = vk::Offset2D{0, 0};
        scissor.extent = m_swapchain_extent;
        command_buffer.setScissor(0, 1, &scissor);

        // need to bind right descriptor sets before draw call
        // descriptor sets are not unique to graphics pipelines
        command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, m_pipeline_layout, 0, 1, &m_descriptor_sets[m_current_frame], 0, nullptr);

        // now we can issue the actual draw command
        // vertex count
        // instance count
        // first index: offset into the vertex buffer
        // first instance
        // vkCmdDraw(command_buffer, static_cast<unsigned>(g_vertices.size()), 1, 0, 0);
        command_buffer.drawIndexed(static_cast<unsigned>(g_indices.size()), 1, 0, 0, 0);

        command_buffer.endRenderPass();

        command_buffer.end();
    }

    void transition_image_layout(vk::Image image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout)
    {
        vk::CommandBuffer command_buffer = begin_single_time_commands();

        vk::PipelineStageFlags source_stage, destination_stage;

        // a common way to perform layout transitions is using an image memory barrier
        // a barrier like that is normally used to sync access to resources,
        // but it can also be used to transition image layouts and transfer queue family ownership
        // when vk::_SHARING_MODE_EXCLUSIVE is used
        // there is an equivalent buffer memory barrier for buffers
        vk::ImageMemoryBarrier barrier{};
        barrier.sType = vk::StructureType::eImageMemoryBarrier;
        barrier.oldLayout = old_layout; // can be vk::_IMAGE_LAYOUT_UNDEFINED if you don't care about original image
        barrier.newLayout = new_layout;

        // these are not the default values
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

        // sub resource range specifies the area of the image that is affected
        // this image is not an array and does not have mipping
        barrier.image = image;
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        // since barriers are used for sync purposes, you must specify which types of operations happen
        // before the barrier and which must wait on the barrier

        if(old_layout == vk::ImageLayout::eUndefined && new_layout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eNone;
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            source_stage = vk::PipelineStageFlagBits::eTopOfPipe;

            // this is not actually a real stage in a graphics or compute pipeline
            // it's more of a pseudo-stage where transfers can happen
            destination_stage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if(old_layout == vk::ImageLayout::eTransferDstOptimal && new_layout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            source_stage = vk::PipelineStageFlagBits::eTransfer;
            destination_stage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        // all types of pipeline barriers are submitted with this command
        // first two params are the stages that should happen before the barrier and stages that have to wait on the barrier
        command_buffer.pipelineBarrier(
                source_stage, destination_stage,
                vk::DependencyFlags(),
                nullptr,
                nullptr,
                barrier
        );

        end_single_time_commands(command_buffer);
    }

    static void populate_debug_messenger_create_info(vk::DebugUtilsMessengerCreateInfoEXT &create_info)
    {
        create_info.sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT;

        // specify severities to call callback for
        create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                      vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;

        // all message types enabled
        create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                  vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        create_info.pfnUserCallback = debug_callback;
        create_info.pUserData = nullptr; // optional
    }

    bool is_device_suitable(vk::PhysicalDevice device)
    {
        QueueFamilyIndices indices = find_queue_families(device);

        bool surface_supported = device.getSurfaceSupportKHR(indices.graphics_family.value(), m_surface);

        if(!surface_supported) return false;

        bool extensions_supported = check_device_extension_support(device);
        bool swap_chain_adequate = false;

        if (extensions_supported)
        {
            SwapChainSupportDetails swap_chain_support = query_swap_chain_support(device);

            swap_chain_adequate = !swap_chain_support.formats.empty() && !swap_chain_support.present_modes.empty();
        }

        return indices.is_complete() && extensions_supported && swap_chain_adequate;
    }

    int rate_device_suitability(vk::PhysicalDevice device)
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

    QueueFamilyIndices find_queue_families(vk::PhysicalDevice device)
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
            vk::Result result = device.getSurfaceSupportKHR(i, m_surface, &present_support);

            if (present_support)
            {
                indices.present_family = i;
            }

            if (indices.is_complete()) break;

            ++i;
        }

        return indices;
    }

    SwapChainSupportDetails query_swap_chain_support(vk::PhysicalDevice device)
    {
        SwapChainSupportDetails details;

        vk::Result result = device.getSurfaceCapabilitiesKHR(m_surface, &details.capabilities);

        unsigned format_count = 0;
        result = device.getSurfaceFormatsKHR(m_surface, &format_count, nullptr);

        if (format_count != 0)
        {
            details.formats.resize(format_count);
            result = device.getSurfaceFormatsKHR(m_surface, &format_count, details.formats.data());
        }

        unsigned present_mode_count = 0;
        result = device.getSurfacePresentModesKHR(m_surface, &present_mode_count, nullptr);

        if (present_mode_count != 0)
        {
            details.present_modes.resize(present_mode_count);
            result = device.getSurfacePresentModesKHR(m_surface, &present_mode_count, details.present_modes.data());
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
    vk::Extent2D choose_swap_extent(const vk::SurfaceCapabilitiesKHR& capabilities)
    {
        if(capabilities.currentExtent.width != std::numeric_limits<unsigned>::max())
        {
            return capabilities.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_window, &width, &height);

            vk::Extent2D actual_extent = { static_cast<unsigned>(width), static_cast<unsigned>(height) };

            // bound extent within limits set by the implementation
            actual_extent.width = std::clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actual_extent.height = std::clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actual_extent;
        }
    }

    // extension stuff
    [[nodiscard]] std::vector<const char*> get_required_extensions() const
    {
        uint32_t extension_count = 0;
        const char** glfw_extensions;
        glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);

        std::vector<const char*> extensions(glfw_extensions, glfw_extensions + extension_count);

        if(m_enable_validation_layers) 
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }

    bool check_validation_layer_support() 
    {
        uint32_t layer_count;

        vk::Result result = vk::enumerateInstanceLayerProperties(&layer_count, nullptr);

        std::vector<vk::LayerProperties> available_layers(layer_count);

        result = vk::enumerateInstanceLayerProperties(&layer_count, available_layers.data());

        for(const char* layer_name : m_validation_layers)
        {
            bool layer_found = false;

            for(const auto& layer_properties : available_layers)
            {
                if(strcmp(layer_name, layer_properties.layerName) == 0)
                {
                    layer_found = true;
                    break;
                }
            }

            if(!layer_found)
            {
                return false;
            }
        }

        return true;
    }

    bool check_device_extension_support(vk::PhysicalDevice device)
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

    vk::ShaderModule create_shader_module(const std::vector<char>& code)
    {
        vk::ShaderModuleCreateInfo create_info{};
        create_info.sType = vk::StructureType::eShaderModuleCreateInfo;
        create_info.codeSize = code.size();
        create_info.pCode = reinterpret_cast<const unsigned*>(code.data());

        vk::ShaderModule shader_module;
        if(m_device.createShaderModule(&create_info, nullptr, &shader_module) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create shader module!");
        }

        // wrappers around shader bytecode
        return shader_module;
    }


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

static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
    auto app = reinterpret_cast<HelloTriangle*>(glfwGetWindowUserPointer(window));
    app->m_framebuffer_resized = true;
}

int main() 
{
    HelloTriangle app;

    try
    {
        app.run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}