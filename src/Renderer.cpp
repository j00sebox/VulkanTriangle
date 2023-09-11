#define VMA_IMPLEMENTATION
#include "Renderer.hpp"
#include "DeviceHelper.hpp"
#include "Vertex.hpp"
#include "Utility.hpp"

#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

struct RecordDrawTask : enki::ITaskSet
{
    void init(Renderer* _renderer, vk::CommandBuffer* _command_buffer, const Scene* _scene, u32 _start, u32 _end, DescriptorSet* _camera_data, DescriptorSet* _material_data)
    {
        renderer = _renderer;
        command_buffer = _command_buffer;
        scene = _scene;
        start = _start;
        end = _end;
        camera_data = _camera_data;
        material_data = _material_data;
    }

    void ExecuteRange(enki::TaskSetPartition range, uint32_t threadnum) override
    {
        for(u32 i = start; i < end; ++i)
        {
            // need to bind right descriptor sets before draw call
            // descriptor sets are not unique to graphics pipelines
            command_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer->get_pipeline_layout(), 1, 1, &material_data->vk_descriptor_set, 0, nullptr);
            command_buffer->bindDescriptorSets(vk::PipelineBindPoint::eGraphics, renderer->get_pipeline_layout(), 0, 1, &camera_data->vk_descriptor_set, 0, nullptr);

            command_buffer->pushConstants(renderer->get_pipeline_layout(), vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4), &scene->models[i].transform);
            command_buffer->pushConstants(renderer->get_pipeline_layout(), vk::ShaderStageFlagBits::eFragment, 64, sizeof(glm::uvec4), scene->models[i].material.textures);

            Buffer* vertex_buffer = renderer->get_buffer(scene->models[i].mesh.vertex_buffer);
            vk::Buffer vertex_buffers[] = {vertex_buffer->vk_buffer};
            vk::DeviceSize offsets[] = {0};
            command_buffer->bindVertexBuffers(0, 1, vertex_buffers, offsets);

            Buffer* index_buffer = renderer->get_buffer(scene->models[i].mesh.index_buffer);
            command_buffer->bindIndexBuffer(index_buffer->vk_buffer, 0, vk::IndexType::eUint32);

            // now we can issue the actual draw command
            // index count
            // instance count
            // first index: offset into the vertex buffer
            // first instance
            command_buffer->drawIndexed(scene->models[i].mesh.index_count, 1, 0, 0, 0);
        }
        command_buffer->end();
    }

    Renderer* renderer;
    vk::CommandBuffer* command_buffer;
    const Scene* scene;
    u32 start;
    u32 end;
    DescriptorSet* camera_data;
    DescriptorSet* material_data;
};


// a descriptor layout specifies the types of resources that are going to be accessed by the pipeline
// a descriptor set specifies the actual buffer or image resources that will be bound to the descriptors
struct CameraData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 camera_position;
};

Renderer::Renderer(GLFWwindow* window, enki::TaskScheduler* scheduler) :
    m_window(window),
    m_scheduler(scheduler),
    m_buffer_pool(&m_pool_allocator, 100, sizeof(Buffer)),
    m_texture_pool(&m_pool_allocator, 10, sizeof(Texture)),
    m_sampler_pool(&m_pool_allocator, 10, sizeof(Sampler)),
    m_descriptor_set_pool(&m_pool_allocator, 10, sizeof(DescriptorSet))
{
    init_instance();
    init_surface();
    init_device();
    init_swapchain();
    init_render_pass();
    init_descriptor_pools();
    init_descriptor_sets();
    init_graphics_pipeline();
    init_command_pools();
    init_depth_resources();
    init_framebuffers();
    init_command_buffers();
    init_sync_objects();

    // create null texture
    m_null_texture = create_texture({
        .format = vk::Format::eR8G8B8A8Srgb,
        .image_src = "../textures/null_texture.png"
    });

    init_imgui();

    configure_lighting({
        .direct_light_colour = {1.f, 1.f, 1.f},
        .direct_light_position = {0.f, 1.f, 0.f}
    });
}

Renderer::~Renderer()
{
    ImGui_ImplVulkan_Shutdown();
    logical_device.destroyDescriptorPool(m_imgui_pool, nullptr);

    for(i32 i = 0; i < s_max_frames_in_flight; ++i)
    {
        // sync objects
        logical_device.destroySemaphore(m_image_available_semaphores[i], nullptr);
        logical_device.destroySemaphore(m_render_finished_semaphores[i], nullptr);
        logical_device.destroyFence(m_in_flight_fences[i], nullptr);

        // uniform buffers
        destroy_buffer(m_camera_buffers[i]);
        destroy_buffer(m_light_buffers[i]);
    }

    destroy_texture(m_null_texture);
    destroy_sampler(m_default_sampler);

    cleanup_swapchain();

    logical_device.destroyDescriptorPool(m_descriptor_pool, nullptr);
    logical_device.destroyDescriptorSetLayout(m_descriptor_set_layout, nullptr);
    logical_device.destroyDescriptorSetLayout(m_camera_data_layout, nullptr);
    logical_device.destroyDescriptorSetLayout(m_texture_set_layout, nullptr);

    vmaDestroyAllocator(m_allocator);

    logical_device.destroyCommandPool(m_main_command_pool);
    logical_device.destroyCommandPool(m_extra_command_pool);
    for(auto& command_pool : m_command_pools)
    {
        logical_device.destroyCommandPool(command_pool, nullptr);
    }
    logical_device.destroyPipeline(m_graphics_pipeline, nullptr);
    logical_device.destroyPipelineLayout(m_pipeline_layout, nullptr);
    logical_device.destroyRenderPass(m_render_pass, nullptr);

    // devices don't interact directly with instances
    logical_device.destroy();

    if (m_enable_validation_layers)
    {
        m_instance.destroyDebugUtilsMessengerEXT(m_debug_messenger, nullptr, m_dispatch_loader);
    }
    m_instance.destroySurfaceKHR(m_surface, nullptr);
    m_instance.destroy();
}

void Renderer::render(Scene* scene)
{
    CameraData camera_data{};
    camera_data.view = scene->camera.camera_look_at();
    camera_data.proj = scene->camera.get_perspective();
    camera_data.camera_position = scene->camera.get_pos();

    // GLM was originally designed for OpenGL where the Y coordinate of the clip space is inverted,
    // so we can remedy this by flipping the sign of the Y scaling factor in the projection matrix
    camera_data.proj[1][1] *= -1;

    // this is the most efficient way to pass constantly changing values to the shader
    // another way to handle smaller data is to use push constants
    auto* camera_buffer = static_cast<Buffer*>(m_buffer_pool.access(m_camera_buffers[m_current_frame]));
    memcpy(camera_buffer->mapped_data, &camera_data, pad_uniform_buffer(sizeof(camera_data)));

    begin_frame();

    auto* material_set = static_cast<DescriptorSet*>(m_descriptor_set_pool.access(m_texture_set));
    auto* camera_set = static_cast<DescriptorSet*>(m_descriptor_set_pool.access(m_camera_sets[m_current_frame]));

    RecordDrawTask record_draw_tasks[m_scheduler->GetNumTaskThreads()];
    u32 models_per_thread, num_recordings, surplus;

    if (m_scheduler->GetNumTaskThreads() > scene->models.size())
    {
        models_per_thread = 1;
        num_recordings = scene->models.size();
        surplus = 0;
    }
    else
    {
        models_per_thread = scene->models.size() / m_scheduler->GetNumTaskThreads();
        num_recordings = m_scheduler->GetNumTaskThreads();
        surplus = scene->models.size() % m_scheduler->GetNumTaskThreads();
    }

	vk::CommandBufferInheritanceInfo inheritance_info{};
	inheritance_info.renderPass = m_render_pass;
	inheritance_info.framebuffer = m_swapchain_framebuffers[m_image_index];
	inheritance_info.subpass = 0;

    u32 start = 0;
    for(u32 i = 0; i < num_recordings; ++i)
    {
        m_command_buffers[m_current_cb_index].begin(inheritance_info);
        m_command_buffers[m_current_cb_index].bind_pipeline(m_graphics_pipeline);

        // since we specified that the viewport and scissor were dynamic we need to do them now
        m_command_buffers[m_current_cb_index].set_viewport(m_swapchain_extent.width, m_swapchain_extent.height);
        m_command_buffers[m_current_cb_index].set_scissor(m_swapchain_extent);

        record_draw_tasks[i].init(this, &m_command_buffers[m_current_cb_index].vk_command_buffer, scene, start, start + models_per_thread, camera_set, material_set);
        m_scheduler->AddTaskSetToPipe(&record_draw_tasks[i]);

        start += models_per_thread;
        m_current_cb_index += s_max_frames_in_flight;
    }

    RecordDrawTask extra_draws;
    if(surplus > 0)
    {
        m_extra_draw_commands[m_current_frame].begin(inheritance_info);
        m_extra_draw_commands[m_current_frame].bind_pipeline(m_graphics_pipeline);
        m_extra_draw_commands[m_current_frame].set_viewport(m_swapchain_extent.width, m_swapchain_extent.height);
        m_extra_draw_commands[m_current_frame].set_scissor(m_swapchain_extent);

        extra_draws.init(this, &m_extra_draw_commands[m_current_frame].vk_command_buffer, scene, start, start + surplus, camera_set, material_set);
        m_scheduler->AddTaskSetToPipe(&extra_draws);
    }

    m_imgui_commands[m_current_frame].begin(inheritance_info);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(),  m_imgui_commands[m_current_frame].vk_command_buffer);
    m_imgui_commands[m_current_frame].end();

    for(u32 i = 0; i < num_recordings; ++i)
    {
        m_scheduler->WaitforTask(&record_draw_tasks[i]);

        if(record_draw_tasks[i].command_buffer)
        {
            m_primary_command_buffers[m_current_frame].vk_command_buffer.executeCommands(1, record_draw_tasks[i].command_buffer);
        }
    }
    if(surplus > 0)
    {
        m_scheduler->WaitforTask(&extra_draws);
        m_primary_command_buffers[m_current_frame].vk_command_buffer.executeCommands(1, extra_draws.command_buffer);
    }
    m_primary_command_buffers[m_current_frame].vk_command_buffer.executeCommands(1, &m_imgui_commands[m_current_frame].vk_command_buffer);

    end_frame();

    m_current_frame = (m_current_frame + 1) % s_max_frames_in_flight;
    m_current_cb_index = m_current_frame;
}

void Renderer::begin_frame()
{
    // takes array of fences and waits for any and all fences
    // saying VK_TRUE means we want to wait for all fences
    // last param is the timeout which we basically disable
    vk::Result result = logical_device.waitForFences(1, &m_in_flight_fences[m_current_frame], true, UINT64_MAX);

    // logical device and swapchain we want to get the image from
    // third param is timeout for image to become available
    // next 2 params are sync objects to be signaled when presentation engine is done with the image
    result = logical_device.acquireNextImageKHR(m_swapchain, UINT64_MAX, m_image_available_semaphores[m_current_frame], nullptr, &m_image_index);

    // if swapchain is not good we immediately recreate and try again in the next frame
    if(result == vk::Result::eErrorOutOfDateKHR)
    {
        recreate_swapchain();
        return;
    }

    // need to reset fences to unsignaled
    // but only reset if we are submitting work
    result = logical_device.resetFences(1, &m_in_flight_fences[m_current_frame]);

    m_primary_command_buffers[m_current_frame].begin();
//    for(u32 i = 0; i < m_scheduler->GetNumTaskThreads(); ++i)
//    {
//        logical_device.resetCommandPool(m_command_pools[i]);
//    }

    // all functions that record commands can be recognized by their vk::Cmd prefix
    // they all return void, so no error handling until the recording is finished
    m_primary_command_buffers[m_current_frame].begin_renderpass(m_render_pass, m_swapchain_framebuffers[m_image_index], m_swapchain_extent, vk::SubpassContents::eSecondaryCommandBuffers);
}

void Renderer::end_frame()
{
    m_primary_command_buffers[m_current_frame].end_renderpass();
    m_primary_command_buffers[m_current_frame].end();

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
    submit_info.pCommandBuffers = &m_primary_command_buffers[m_current_frame].vk_command_buffer;

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
    present_info.pImageIndices = &m_image_index;

    // one last optional param
    // can get an array of vk::Result to check every swapchain to see if presentation was successful
    present_info.pResults = nullptr;

    vk::Result result = m_present_queue.presentKHR(&present_info);

    if(result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        recreate_swapchain();
    }
}

void Renderer::recreate_swapchain()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window, &width, &height);
        glfwWaitEvents();
    }

    wait_for_device_idle();
    cleanup_swapchain();
    init_swapchain();
    init_depth_resources();
    init_framebuffers();
}

void Renderer::configure_lighting(LightingData data)
{
    m_light_data = data;
    for(int i = 0; i < s_max_frames_in_flight; ++i)
    {
        auto* buffer = static_cast<Buffer*>(m_buffer_pool.access(m_light_buffers[i]));
        memcpy(buffer->mapped_data, &m_light_data, pad_uniform_buffer(sizeof(m_light_data)));
    }
}

void Renderer::init_instance()
{
    if (m_enable_validation_layers && !check_validation_layer_support())
    {
        throw std::runtime_error("validation layers requested, but not available");
    }

    // a lot of info is passed through structs
    vk::ApplicationInfo app_info{};
    app_info.sType = vk::StructureType::eApplicationInfo;
    app_info.pApplicationName = "Hello Triangle";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "No Engine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    vk::InstanceCreateInfo create_info{};
    create_info.sType =  vk::StructureType::eInstanceCreateInfo;
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
        debug_create_info.sType = vk::StructureType::eDebugUtilsMessengerCreateInfoEXT;

        // specify severities to call callback for
        debug_create_info.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;

        // all message types enabled
        debug_create_info.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        debug_create_info.pfnUserCallback = debug_callback;
        debug_create_info.pUserData = nullptr; // optional
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

    // setup debug messenger
    if(m_enable_validation_layers)
    {
        m_debug_messenger = m_instance.createDebugUtilsMessengerEXT(debug_create_info, nullptr, m_dispatch_loader);
    }
}

void Renderer::init_surface()
{
    VkSurfaceKHR c_style_surface;
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &c_style_surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = c_style_surface;
}

void Renderer::init_device()
{
    vk::PhysicalDeviceFeatures physical_device_features{};
    physical_device_features.samplerAnisotropy = true;

    // setup for bindless resources
    vk::PhysicalDeviceVulkan12Features physical_device_features12{};
    physical_device_features12.sType = vk::StructureType::ePhysicalDeviceVulkan12Features;
    physical_device_features12.descriptorBindingSampledImageUpdateAfterBind = true;
    physical_device_features12.descriptorBindingStorageImageUpdateAfterBind = true;
    physical_device_features12.descriptorBindingStorageBufferUpdateAfterBind = true;
    physical_device_features12.descriptorIndexing = true;
    physical_device_features12.descriptorBindingPartiallyBound = true;
    physical_device_features12.runtimeDescriptorArray = true;
    physical_device_features12.shaderSampledImageArrayNonUniformIndexing = true;

    vk::DeviceCreateInfo create_info{};
    create_info.sType = vk::StructureType::eDeviceCreateInfo;

    create_info.pEnabledFeatures = &physical_device_features;
    create_info.pNext = &physical_device_features12;

    if (m_enable_validation_layers)
    {
        create_info.enabledLayerCount = static_cast<unsigned>(m_validation_layers.size());
        create_info.ppEnabledLayerNames = m_validation_layers.data();
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }

    m_physical_device = DeviceHelper::pick_physical_device(m_instance);
    QueueFamilyIndices indices = DeviceHelper::find_queue_families(m_physical_device, m_surface);

    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    std::set<unsigned> unique_queue_families = {indices.graphics_family.value(), indices.present_family.value(), indices.transfer_family.value()};

    float queue_priority = 1.f;
    for (unsigned queue_family: unique_queue_families)
    {
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
    if (m_physical_device.createDevice(&create_info, nullptr, &logical_device) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create logical device!");
    }

    // since we only set 1 queue we can just index at 0
    logical_device.getQueue(indices.graphics_family.value(), 0, &m_graphics_queue);
    logical_device.getQueue(indices.present_family.value(), 0, &m_present_queue);
    logical_device.getQueue(indices.transfer_family.value(), 0, &m_transfer_queue);

    m_physical_device.getProperties(&m_device_properties);

    VmaAllocatorCreateInfo vma_info{};
    vma_info.vulkanApiVersion = m_device_properties.apiVersion;
    vma_info.physicalDevice = m_physical_device;
    vma_info.device = logical_device;
    vma_info.instance = m_instance;

    vmaCreateAllocator(&vma_info, &m_allocator);
}

void Renderer::create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image &image, vk::DeviceMemory &image_memory)
{
    vk::ImageCreateInfo image_info{};
    image_info.sType = vk::StructureType::eImageCreateInfo;
    image_info.imageType = vk::ImageType::e2D; // what kind of coordinate system to use
    image_info.extent.width = static_cast<u32>(width);
    image_info.extent.height = static_cast<u32>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;

    // use the same format for the texels as the pixels in the buffer
    image_info.format = format;

    // defines how the texels are laid out in memory
    image_info.tiling = tiling;

    // since we are transitioning the image to a new place anyway
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    // image will be used as a destination to copy the pixel data to
    // also want to be able to access the image from the shader
    image_info.usage = usage;

    // will only be used by one queue family
    image_info.sharingMode = vk::SharingMode::eExclusive;

    // images used as textures don't really need to be multisampled
    image_info.samples = vk::SampleCountFlagBits::e1;

    if(logical_device.createImage(&image_info, nullptr, &image) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create image!");
    }

    vk::MemoryRequirements memory_requirements;

    logical_device.getImageMemoryRequirements(image, &memory_requirements);

    vk::MemoryAllocateInfo alloc_info{};
    alloc_info.sType = vk::StructureType::eMemoryAllocateInfo;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = DeviceHelper::find_memory_type(memory_requirements.memoryTypeBits, properties, m_physical_device);

    if(logical_device.allocateMemory(&alloc_info, nullptr, &image_memory) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to allocate image memory!");
    }

    logical_device.bindImageMemory(image, image_memory, 0);
}

vk::ImageView Renderer::create_image_view(const vk::Image& image, vk::Format format, vk::ImageAspectFlags image_aspect)
{
    vk::ImageViewCreateInfo create_info{};
    create_info.sType = vk::StructureType::eImageViewCreateInfo;
    create_info.image = image;

    // how image data should be interpreted
    // 1D textures, 2D textures, 3D textures and cube maps
    create_info.viewType = vk::ImageViewType::e2D;
    create_info.format = format;

    // allows swizzling of the colour channels
    // sticking to default mapping
    create_info.components.r = vk::ComponentSwizzle::eIdentity;
    create_info.components.g = vk::ComponentSwizzle::eIdentity;
    create_info.components.b = vk::ComponentSwizzle::eIdentity;
    create_info.components.a = vk::ComponentSwizzle::eIdentity;

    // describes what the image's purpose is and which part of the image should be used
    // these images will be used as colour targets without any mipmapping
    create_info.subresourceRange.aspectMask = image_aspect;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    vk::ImageView image_view;
    if(logical_device.createImageView(&create_info, nullptr, &image_view) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create image view!");
    }

    return image_view;
}

u32 Renderer::create_buffer(const BufferCreationInfo& buffer_creation)
{
    u32 handle = m_buffer_pool.acquire();
    auto* buffer = static_cast<Buffer*>(m_buffer_pool.access(handle));

    buffer->size = buffer_creation.size;

    vk::BufferCreateInfo buffer_info{};
    buffer_info.sType = vk::StructureType::eBufferCreateInfo;
    buffer_info.size = buffer_creation.size;
    buffer_info.usage = buffer_creation.usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    VmaAllocationCreateInfo memory_info{};
    memory_info.flags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
    memory_info.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;

    if(buffer_creation.persistent)
    {
        memory_info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocationInfo allocation_info{};
    vmaCreateBuffer(m_allocator, reinterpret_cast<const VkBufferCreateInfo*>(&buffer_info), &memory_info,
                    &buffer->vk_buffer, &buffer->vma_allocation, &allocation_info);

    if(buffer_creation.data)
    {
        void* data;
        vmaMapMemory(m_allocator, buffer->vma_allocation, &data);
        memcpy(data, buffer_creation.data, (size_t)buffer_creation.size);
        vmaUnmapMemory(m_allocator, buffer->vma_allocation);
    }

    if(buffer_creation.persistent)
    {
        buffer->mapped_data = static_cast<u8*>(allocation_info.pMappedData);
    }

    return handle;
}

u32 Renderer::create_texture(const TextureCreationInfo& texture_creation)
{
    if(m_texture_map.contains(texture_creation.image_src))
    {
        return m_texture_map[texture_creation.image_src];
    }

    u32 handle = m_texture_pool.acquire();
    auto* texture = static_cast<Texture*>(m_texture_pool.access(handle));

    stbi_set_flip_vertically_on_load(1);

    int width, height, channels;
    stbi_uc* pixels = stbi_load(texture_creation.image_src, &width, &height, &channels, STBI_rgb_alpha);

    vk::DeviceSize image_size = width * height * 4;

    texture->width = width;
    texture->height = height;
    texture->name = texture_creation.image_src;

    if(!pixels)
    {
        throw std::runtime_error("failed to load texture image!");
    }

    u32 staging_handle = create_buffer({
       .usage = vk::BufferUsageFlagBits::eTransferSrc,
       .size = (u32)image_size,
       .data = pixels
    });

    auto* staging_buffer = static_cast<Buffer*>(m_buffer_pool.access(staging_handle));

    stbi_image_free(pixels);

    vk::ImageCreateInfo image_info{};
    image_info.sType = vk::StructureType::eImageCreateInfo;
    image_info.imageType = vk::ImageType::e2D; // what kind of coordinate system to use
    image_info.extent.width = static_cast<u32>(width);
    image_info.extent.height = static_cast<u32>(height);
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;

    // use the same format for the texels as the pixels in the buffer
    image_info.format = texture_creation.format;

    // defines how the texels are laid out in memory
    image_info.tiling = vk::ImageTiling::eOptimal;

    // since we are transitioning the image to a new place anyway
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    // image will be used as a destination to copy the pixel data to
    // also want to be able to access the image from the shader
    image_info.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;

    // will only be used by one queue family
    image_info.sharingMode = vk::SharingMode::eExclusive;

    // images used as textures don't really need to be multisampled
    image_info.samples = vk::SampleCountFlagBits::e1;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(m_allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info), &memory_info, &texture->vk_image, &texture->vma_allocation, nullptr);

    transition_image_layout(texture->vk_image, texture_creation.format, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

    copy_buffer_to_image(staging_buffer->vk_buffer, texture->vk_image, static_cast<unsigned>(width), static_cast<unsigned>(height));

    transition_image_layout(texture->vk_image, texture_creation.format, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);

    texture->vk_image_view = create_image_view(texture->vk_image, texture_creation.format, vk::ImageAspectFlagBits::eColor);

    destroy_buffer(staging_handle);

    m_texture_map[texture_creation.image_src] = handle;

    return handle;
}

u32 Renderer::create_sampler(const SamplerCreationInfo& sampler_creation)
{
    u32 sampler_handle = m_sampler_pool.acquire();
    auto* sampler = static_cast<Sampler*>(m_sampler_pool.access(sampler_handle));

    vk::SamplerCreateInfo sampler_info{};
    sampler_info.sType = vk::StructureType::eSamplerCreateInfo;
    sampler_info.magFilter = sampler_creation.mag_filter;
    sampler_info.minFilter = sampler_creation.min_filter;

    sampler_info.addressModeU = sampler_creation.u_mode;
    sampler_info.addressModeV = sampler_creation.v_mode;
    sampler_info.addressModeW = sampler_creation.w_mode;

    vk::PhysicalDeviceProperties properties{};
    m_physical_device.getProperties(&properties);

    sampler_info.anisotropyEnable = true;
    sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    sampler_info.borderColor = vk::BorderColor::eIntOpaqueBlack;

    sampler_info.unnormalizedCoordinates = false;

    sampler_info.compareEnable = false;
    sampler_info.compareOp = vk::CompareOp::eAlways;

    sampler_info.mipmapMode = vk::SamplerMipmapMode::eLinear;
    sampler_info.mipLodBias = 0.f;
    sampler_info.minLod = 0.f;
    sampler_info.maxLod = 0.f;

    if(logical_device.createSampler(&sampler_info, nullptr, &sampler->vk_sampler) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create texture sampler!");
    }

    return sampler_handle;
}

u32 Renderer::create_descriptor_set(const DescriptorSetCreationInfo& descriptor_set_creation)
{
    u32 descriptor_set_handle = m_descriptor_set_pool.acquire();
    auto* descriptor_set = static_cast<DescriptorSet*>(m_descriptor_set_pool.access(descriptor_set_handle));

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
    allocInfo.descriptorPool = m_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptor_set_creation.layout;

    if(vk::Result result = logical_device.allocateDescriptorSets(&allocInfo, &descriptor_set->vk_descriptor_set); result != vk::Result::eSuccess)
    {
        std::cout << (int)result << std::endl;
        throw std::runtime_error("failed to create descriptor set!");
    }

    vk::WriteDescriptorSet descriptor_writes[descriptor_set_creation.num_resources];

    for(int i = 0; i < descriptor_set_creation.num_resources; ++i)
    {
        switch(descriptor_set_creation.types[i])
        {
            case vk::DescriptorType::eUniformBuffer:
            {
                auto* buffer = static_cast<Buffer*>(m_buffer_pool.access(descriptor_set_creation.resource_handles[i]));

                vk::DescriptorBufferInfo descriptor_info{};
                descriptor_info.buffer = buffer->vk_buffer;
                descriptor_info.offset = 0;
                descriptor_info.range = buffer->size;

                descriptor_writes[i].sType = vk::StructureType::eWriteDescriptorSet;
                descriptor_writes[i].dstSet = descriptor_set->vk_descriptor_set;
                descriptor_writes[i].dstBinding = descriptor_set_creation.bindings[i]; // index binding
                descriptor_writes[i].dstArrayElement = 0;

                descriptor_writes[i].descriptorType = vk::DescriptorType::eUniformBuffer;
                descriptor_writes[i].descriptorCount = 1; // how many array elements to update

                descriptor_writes[i].pBufferInfo = &descriptor_info;

                // used for descriptors that reference image data
                descriptor_writes[i].pImageInfo = nullptr;
                descriptor_writes[i].pTexelBufferView = nullptr;

                logical_device.updateDescriptorSets(1, &descriptor_writes[i], 0, nullptr);

               break;
            }
            case vk::DescriptorType::eCombinedImageSampler:
            {
                auto* texture = static_cast<Texture*>(m_texture_pool.access(descriptor_set_creation.resource_handles[i]));
                auto* sampler = static_cast<Sampler*>(m_sampler_pool.access(descriptor_set_creation.sampler_handles[i]));

                vk::DescriptorImageInfo descriptor_info{};
                descriptor_info.imageView = texture->vk_image_view;
                descriptor_info.sampler = sampler->vk_sampler;
                descriptor_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

                descriptor_writes[i].sType = vk::StructureType::eWriteDescriptorSet;
                descriptor_writes[i].dstSet = descriptor_set->vk_descriptor_set; // descriptor set to update
                descriptor_writes[i].dstBinding = descriptor_set_creation.bindings[i];
                descriptor_writes[i].dstArrayElement = 0;
                descriptor_writes[i].descriptorType = vk::DescriptorType::eCombinedImageSampler;
                descriptor_writes[i].descriptorCount = 1; // how many array elements to update
                descriptor_writes[i].pImageInfo = &descriptor_info;

                logical_device.updateDescriptorSets(1, &descriptor_writes[i], 0, nullptr);

                break;
            }
        }
    }

    // m_logical_device.updateDescriptorSets(descriptor_set_creation.num_resources, descriptor_writes, 0, nullptr);

    return descriptor_set_handle;
}

void Renderer::create_image(u32 width, u32 height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::Image& image, VmaAllocation& image_vma)
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
    image_info.format = format; 

    // defines how the texels are laid out in memory
    image_info.tiling = tiling;

    // since we are transitioning the image to a new place anyway
    image_info.initialLayout = vk::ImageLayout::eUndefined;

    // image will be used as a destination to copy the pixel data to
    // also want to be able to access the image from the shader
    image_info.usage = usage;

    // will only be used by one queue family
    image_info.sharingMode = vk::SharingMode::eExclusive;

    // images used as textures don't really need to be multisampled
    image_info.samples = vk::SampleCountFlagBits::e1;

    VmaAllocationCreateInfo memory_info{};
    memory_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(m_allocator, reinterpret_cast<const VkImageCreateInfo*>(&image_info), &memory_info, reinterpret_cast<VkImage*>(&image), &image_vma, nullptr);
}

vk::ShaderModule Renderer::create_shader_module(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo create_info{};
    create_info.sType = vk::StructureType::eShaderModuleCreateInfo;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const unsigned*>(code.data());

    vk::ShaderModule shader_module;
    if(logical_device.createShaderModule(&create_info, nullptr, &shader_module) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create shader module!");
    }

    // wrappers around shader bytecode
    return shader_module;
}

void Renderer::update_texture_set(u32* texture_handles, u32 num_textures)
{
    auto* texture_set = static_cast<DescriptorSet*>(m_descriptor_set_pool.access(m_texture_set));

    // TODO: make it update all at once
    for(i32 i = 0; i < num_textures; ++i)
    {
        auto* texture = static_cast<Texture*>(m_texture_pool.access(texture_handles[i]));
        auto* sampler = static_cast<Sampler*>(m_sampler_pool.access(m_default_sampler)); // FIXME: magic number

        vk::DescriptorImageInfo descriptor_info{};
        descriptor_info.imageView = texture->vk_image_view;
        descriptor_info.sampler = sampler->vk_sampler;
        descriptor_info.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        vk::WriteDescriptorSet descriptor_write{};
        descriptor_write.sType = vk::StructureType::eWriteDescriptorSet;
        descriptor_write.dstSet = texture_set->vk_descriptor_set; // descriptor set to update
        descriptor_write.dstBinding = 10; // FIXME: magic number
        descriptor_write.dstArrayElement = texture_handles[i];
        descriptor_write.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        descriptor_write.descriptorCount = 1; // how many array elements to update
        descriptor_write.pImageInfo = &descriptor_info;

        logical_device.updateDescriptorSets(1, &descriptor_write, 0, nullptr);
    }
}

void Renderer::destroy_buffer(u32 buffer_handle)
{
    auto* buffer = static_cast<Buffer*>(m_buffer_pool.access(buffer_handle));
    vmaDestroyBuffer(m_allocator, buffer->vk_buffer, buffer->vma_allocation);
    m_buffer_pool.free(buffer_handle);
}

void Renderer::destroy_texture(u32 texture_handle)
{
    if(!m_texture_pool.valid_handle(texture_handle))
    {
        return;
    }

    auto* texture = static_cast<Texture*>(m_texture_pool.access(texture_handle));
    logical_device.destroyImageView(texture->vk_image_view, nullptr);
    vmaDestroyImage(m_allocator, texture->vk_image, texture->vma_allocation);
    m_texture_pool.free(texture_handle);
    m_texture_map.erase(texture->name);
}

void Renderer::destroy_sampler(u32 sampler_handle)
{
    auto* sampler = static_cast<Sampler*>(m_sampler_pool.access(sampler_handle));
    logical_device.destroySampler(sampler->vk_sampler, nullptr);
    m_sampler_pool.free(sampler_handle);
}

void Renderer::init_swapchain()
{
    SwapChainSupportDetails swap_chain_support = DeviceHelper::query_swap_chain_support(m_physical_device, m_surface);

    // get size for framebuffers
    int width, height;
    glfwGetFramebufferSize(m_window, &width, &height);
    vk::Extent2D extent = { static_cast<unsigned>(width), static_cast<unsigned>(height) };

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
    create_info.sType = vk::StructureType::eSwapchainCreateInfoKHR;
    create_info.surface = m_surface;

    // swap chain image details
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = actual_extent;
    create_info.imageArrayLayers = 1; // amount of layers each image consists of
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = DeviceHelper::find_queue_families(m_physical_device, m_surface);
    unsigned queue_family_indices[] = { indices.graphics_family.value(), indices.present_family.value() };

    if(indices.graphics_family != indices.present_family)
    {
        // images can be used across multiple queue families without explicit ownership transfers
        // requires to state which queue families will be shared
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        // image owned by one queue family
        // ownership must be explicitly transferred before using it in another one
        create_info.imageSharingMode = vk::SharingMode::eExclusive;

        // optional
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    // can set up some kind of transform you want to apply to the images
    // set it to current transform to prevent that
    create_info.preTransform = swap_chain_support.capabilities.currentTransform;

    // specifies if alpha field should be used for blending
    // almost always want to ignore the alpha channel
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE; // don't care about the colour of the pixels being obscured by another window

    // if the swap chain becomes invalid during runtime (ex. window resize)
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if(logical_device.createSwapchainKHR(&create_info, nullptr, &m_swapchain) != vk::Result::eSuccess)
    {
        throw std::runtime_error("unable to create swapchain!");
    }

    // we only specified a minimum amount of images to create, the implementation is capable of making more
    vk::Result result = logical_device.getSwapchainImagesKHR(m_swapchain, &image_count, nullptr);
    m_swapchain_images.resize(image_count);

    result = logical_device.getSwapchainImagesKHR(m_swapchain, &image_count, m_swapchain_images.data());

    // need these for later
    m_swapchain_image_format = surface_format.format;
    m_swapchain_extent = actual_extent;

    m_swapchain_image_views.resize(m_swapchain_images.size());

    for(size_t i = 0; i < m_swapchain_images.size(); ++i)
    {
        m_swapchain_image_views[i] = create_image_view(m_swapchain_images[i], m_swapchain_image_format, vk::ImageAspectFlagBits::eColor);
    }
}

void Renderer::init_render_pass()
{
    // in this case we just have a single colour buffer
    vk::AttachmentDescription colour_attachment{};
    colour_attachment.format = m_swapchain_image_format;
    colour_attachment.samples = vk::SampleCountFlagBits::e1;

    // these determine what to do with the data in the attachment before and after rendering
    // applies to both colour and depth
    colour_attachment.loadOp = vk::AttachmentLoadOp::eClear; // clear at start
    colour_attachment.storeOp = vk::AttachmentStoreOp::eStore; // rendered pixels wil be stored

    // not using stencil now
    colour_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    colour_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;

    // textures are represented by VKImage objects with a certain pixel format
    // initial layout specifies what layout the image will have before the render pass
    // final layout is what to transition to after the render pass
    // we set the initial layout as undefined because we don't care what the previous layout was before
    // we want the image to be ready for presentation using the swap chain hence the final layout
    colour_attachment.initialLayout = vk::ImageLayout::eUndefined;
    colour_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentDescription depth_attachment{};
    depth_attachment.format = vk::Format::eD32Sfloat;
    depth_attachment.samples = vk::SampleCountFlagBits::e1;
    depth_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    depth_attachment.storeOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    depth_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    depth_attachment.initialLayout = vk::ImageLayout::eUndefined;
    depth_attachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // a render pass can have multiple subpasses
    // they are subsequent passes that depend on the contents of framebuffers in previous passes
    // every subpass references one or more previously described attachments
    vk::AttachmentReference colour_attachment_ref{};
    colour_attachment_ref.attachment = 0; // references attachment by index
    colour_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal; // layout attachment should have during subpass

    vk::AttachmentReference depth_attachment_ref{};
    depth_attachment_ref.attachment = 1;
    depth_attachment_ref.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    vk::SubpassDescription subpass{};
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colour_attachment_ref;
    subpass.pDepthStencilAttachment = &depth_attachment_ref;

    // subpasses in a render pass automatically take care of image layout transitions
    // these transitions are controlled by subpass dependencies
    // since we only have 1 subpass, the operations before and after are implicit subpasses
    vk::SubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL; // refers to implicit subpass before
    dependency.dstSubpass = 0; // index to our subpass

    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.srcAccessMask = vk::AccessFlagBits::eNone;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

    std::array<vk::AttachmentDescription, 2> attachments = {colour_attachment, depth_attachment};
    vk::RenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = vk::StructureType::eRenderPassCreateInfo;
    render_pass_info.attachmentCount = static_cast<unsigned>(attachments.size());
    render_pass_info.pAttachments = attachments.data();
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    if(logical_device.createRenderPass(&render_pass_info, nullptr, &m_render_pass) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void Renderer::init_descriptor_sets()
{
    vk::DescriptorSetLayoutBinding camera_data_layout_binding{};
    camera_data_layout_binding.binding = 0; // binding in the shader
    camera_data_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    camera_data_layout_binding.descriptorCount = 1;
    camera_data_layout_binding.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
    camera_data_layout_binding.pImmutableSamplers = nullptr; // only relevant for image sampling descriptors

    vk::DescriptorSetLayoutBinding lighting_data_layout_binding{};
    lighting_data_layout_binding.binding = 1;
    lighting_data_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    lighting_data_layout_binding.descriptorCount = 1;
    lighting_data_layout_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;
    lighting_data_layout_binding.pImmutableSamplers = nullptr;

    vk::DescriptorSetLayoutBinding bindings[] = { camera_data_layout_binding, lighting_data_layout_binding };

    vk::DescriptorSetLayoutCreateInfo uniform_layout_info{};
    uniform_layout_info.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
    uniform_layout_info.bindingCount = 2;
    uniform_layout_info.pBindings = bindings;

    if(logical_device.createDescriptorSetLayout(&uniform_layout_info, nullptr, &m_camera_data_layout) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    // create the buffers for the view/projection transforms
    u32 camera_buffer_size = pad_uniform_buffer(sizeof(CameraData));
    u32 light_buffer_size = pad_uniform_buffer(sizeof(LightingData));

    // TODO: combine into one big buffer
    for(int i = 0; i < s_max_frames_in_flight; ++i)
    {
        m_camera_buffers[i] = create_buffer({
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .size = camera_buffer_size,
            .persistent = true
        });

        m_light_buffers[i] = create_buffer({
            .usage = vk::BufferUsageFlagBits::eUniformBuffer,
            .size = light_buffer_size,
            .persistent = true
        });
    }

    m_camera_sets.resize(s_max_frames_in_flight);

    // create the sets for the camera
    for(int i = 0; i < s_max_frames_in_flight; ++i)
    {
        m_camera_sets[i] = create_descriptor_set({
           .resource_handles = {m_camera_buffers[i], m_light_buffers[i]},
           .bindings = {0, 1},
           .types = {vk::DescriptorType::eUniformBuffer, vk::DescriptorType::eUniformBuffer},
           .layout = m_camera_data_layout,
           .num_resources = 2,
        });
    }

    // bindless texture set layout
    // FIXME: magic numbers
    vk::DescriptorSetLayoutBinding image_sampler_binding{};
    image_sampler_binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
    image_sampler_binding.descriptorCount = k_max_bindless_resources; // max bindless resources
    image_sampler_binding.binding = 10; // binding for all bindless textures (paradox)
    image_sampler_binding.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutBinding storage_image_binding{};
    storage_image_binding.descriptorType = vk::DescriptorType::eStorageImage;
    storage_image_binding.descriptorCount = k_max_bindless_resources;
    storage_image_binding.binding = 11;

    vk::DescriptorSetLayoutBinding bindless_bindings[] = { image_sampler_binding, storage_image_binding };

    vk::DescriptorSetLayoutCreateInfo bindless_layout_create_info{};
    bindless_layout_create_info.sType = vk::StructureType::eDescriptorSetLayoutCreateInfo;
    bindless_layout_create_info.bindingCount = 2;
    bindless_layout_create_info.pBindings = bindless_bindings;
    bindless_layout_create_info.flags = vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPoolEXT;

    vk::DescriptorBindingFlags bindless_flags = vk::DescriptorBindingFlagBits::ePartiallyBound | vk::DescriptorBindingFlagBits::eUpdateAfterBind;

    vk::DescriptorBindingFlags binding_flags[] = { bindless_flags, bindless_flags };
    vk::DescriptorSetLayoutBindingFlagsCreateInfoEXT extended_info{};
    extended_info.sType = vk::StructureType::eDescriptorSetLayoutBindingFlagsCreateInfoEXT;
    extended_info.bindingCount = 2;
    extended_info.pBindingFlags = binding_flags;

    bindless_layout_create_info.pNext = &extended_info;

    if(logical_device.createDescriptorSetLayout(&bindless_layout_create_info, nullptr, &m_texture_set_layout) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    m_texture_set = m_descriptor_set_pool.acquire();
    auto* descriptor_set = static_cast<DescriptorSet*>(m_descriptor_set_pool.access(m_texture_set));

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = vk::StructureType::eDescriptorSetAllocateInfo;
    allocInfo.descriptorPool = m_descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_texture_set_layout;

    if(vk::Result result = logical_device.allocateDescriptorSets(&allocInfo, &descriptor_set->vk_descriptor_set); result != vk::Result::eSuccess)
    {
        std::cout << (int)result << std::endl;
        throw std::runtime_error("failed to create descriptor set!");
    }

    m_default_sampler = create_sampler({
        .min_filter = vk::Filter::eLinear,
        .mag_filter = vk::Filter::eLinear,
        .u_mode = vk::SamplerAddressMode::eRepeat,
        .v_mode = vk::SamplerAddressMode::eRepeat,
        .w_mode = vk::SamplerAddressMode::eRepeat
    });
}

void Renderer::init_graphics_pipeline()
{
    bool cache_exists = std::filesystem::exists("pipeline_cache.bin");
    bool cache_header_valid = false;

    vk::PipelineCache pipeline_cache = nullptr;

    vk::PipelineCacheCreateInfo pipeline_cache_info{};
    pipeline_cache_info.sType = vk::StructureType::ePipelineCacheCreateInfo;

    if(cache_exists)
    {
        std::vector<u8> pipeline_cache_data = util::read_binary_file("pipeline_cache.bin");

        // if there is a new driver version there is a chance that it won't be able to make use of the old cache file
        // need to check some cache header details and compare them to our physical device
        // if they match, create the cache like normal, otherwise need to overwrite it
        auto* cache_header = (vk::PipelineCacheHeaderVersionOne*)pipeline_cache_data.data();
        cache_header_valid = (cache_header->deviceID == m_device_properties.deviceID &&
                cache_header->vendorID == m_device_properties.vendorID &&
                memcmp(cache_header->pipelineCacheUUID, m_device_properties.pipelineCacheUUID, VK_UUID_SIZE) == 0);

        if(cache_header_valid)
        {
            pipeline_cache_info.pInitialData = pipeline_cache_data.data();
            pipeline_cache_info.initialDataSize = pipeline_cache_data.size();
        }
    }

    if(logical_device.createPipelineCache(&pipeline_cache_info, nullptr, &pipeline_cache) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to create pipeline cache!");
    }

    auto vert_shader_code = util::read_binary_file("../shaders/vert.spv");
    auto frag_shader_code = util::read_binary_file("../shaders/frag.spv");

    // TODO: do some thing with this
    util::spirv::ParseResult parse_result{};
    util::spirv::parse_binary((u32*)vert_shader_code.data(), vert_shader_code.size(), parse_result);

    vk::ShaderModule vert_shader_module = create_shader_module(vert_shader_code);
    vk::ShaderModule frag_shader_module = create_shader_module(frag_shader_code);

    // to use these shaders we need to assign them to a specific pipeline stage
    vk::PipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = vk::StructureType::ePipelineShaderStageCreateInfo;
    vert_shader_stage_info.stage = vk::ShaderStageFlagBits::eVertex;

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

    vk::PipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = vk::StructureType::ePipelineDepthStencilStateCreateInfo;
    depth_stencil.depthTestEnable = true;
    depth_stencil.depthWriteEnable = true;
    depth_stencil.depthCompareOp = vk::CompareOp::eLess;
    depth_stencil.depthBoundsTestEnable = false;
    depth_stencil.minDepthBounds = 0.f;
    depth_stencil.maxDepthBounds = 1.f;
    depth_stencil.stencilTestEnable = false;

    vk::PipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = vk::StructureType::ePipelineLayoutCreateInfo;

    // need to specify the descriptor set layout here
    pipeline_layout_info.setLayoutCount = 2;
    vk::DescriptorSetLayout layouts[] = {m_camera_data_layout, m_texture_set_layout};
    pipeline_layout_info.pSetLayouts = layouts;

    // need to tell the pipeline that there will be a push constant coming in
    vk::PushConstantRange model_push_constant_info{};
    model_push_constant_info.offset = 0;
    model_push_constant_info.size = sizeof(glm::mat4);
    model_push_constant_info.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::PushConstantRange texture_push_constant_info{};
    texture_push_constant_info.offset = 64;
    texture_push_constant_info.size = sizeof(glm::uvec4);
    texture_push_constant_info.stageFlags = vk::ShaderStageFlagBits::eFragment;

    vk::PushConstantRange push_constant_ranges[] = { model_push_constant_info, texture_push_constant_info };
    pipeline_layout_info.pushConstantRangeCount = 2;
    pipeline_layout_info.pPushConstantRanges = push_constant_ranges;

    if(logical_device.createPipelineLayout(&pipeline_layout_info, nullptr, &m_pipeline_layout) != vk::Result::eSuccess)
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

    pipeline_info.pDepthStencilState = &depth_stencil;

    // Vulkan allows you to create new pipelines derived from existing ones
    // we don't need this now so just set to null
    pipeline_info.basePipelineHandle = nullptr;
    pipeline_info.basePipelineIndex = -1;

    // you can actually use this one function call to make multiple pipelines
    // the second param references a vk::PipelineCache object
    if(logical_device.createGraphicsPipelines(pipeline_cache, 1, &pipeline_info, nullptr, &m_graphics_pipeline) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    if(!cache_exists || !cache_header_valid)
    {
        size_t cache_data_size = 0;

        if(logical_device.getPipelineCacheData(pipeline_cache, &cache_data_size, nullptr) != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to fetch pipeline cache data!");
        }

        void* cache_data = malloc(cache_data_size);

        if(logical_device.getPipelineCacheData(pipeline_cache, &cache_data_size, cache_data) != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to fetch pipeline cache data!");
        }

        util::write_binary_file(cache_data, cache_data_size, "pipeline_cache.bin");
        free(cache_data);
    }

    logical_device.destroyPipelineCache(pipeline_cache, nullptr);
    logical_device.destroyShaderModule(vert_shader_module, nullptr);
    logical_device.destroyShaderModule(frag_shader_module, nullptr);
}

void Renderer::init_command_pools()
{
    QueueFamilyIndices queue_family_indices = DeviceHelper::find_queue_families(m_physical_device, m_surface);

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.sType = vk::StructureType::eCommandPoolCreateInfo;

    // allows command buffers to be rerecorded individually
    // without this flag they all have to be reset together
    // we will be recording a command buffer every frame, so we want to be able to write to it
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    // command buffers are executed by submitting them on one of the device queues
    // each command pool can only allocate command buffers that will be submitted to the same type of queue
    // since we're recording commands for drawing we make it the graphics queue
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

    if(logical_device.createCommandPool(&pool_info, nullptr, &m_main_command_pool) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create command pool!");
    }

    if(logical_device.createCommandPool(&pool_info, nullptr, &m_extra_command_pool) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create command pool!");
    }

    m_command_pools.resize(m_scheduler->GetNumTaskThreads());

    for(auto& command_pool : m_command_pools)
    {
        if(logical_device.createCommandPool(&pool_info, nullptr, &command_pool) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create command pool!");
        }
    }

    pool_info.queueFamilyIndex = queue_family_indices.transfer_family.value();
}

void Renderer::init_command_buffers()
{
    m_command_buffers.resize(m_command_pools.size() * 3);

    u32 command_buffer_index = 0;
    for(const auto& command_pool : m_command_pools)
    {
        // now need to allocate the secondary command buffers for this pool
        vk::CommandBufferAllocateInfo secondary_alloc_info{};
        secondary_alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
        secondary_alloc_info.commandPool = command_pool;
        secondary_alloc_info.level = vk::CommandBufferLevel::eSecondary;
        secondary_alloc_info.commandBufferCount = 1;

        for(u32 i = 0; i < s_max_frames_in_flight; ++i)
        {
            if(logical_device.allocateCommandBuffers(&secondary_alloc_info, &m_command_buffers[i + command_buffer_index].vk_command_buffer) != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to allocate command buffers!");
            }
        }

        command_buffer_index += 3;
    }

    vk::CommandBufferAllocateInfo primary_alloc_info{};
    primary_alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
    primary_alloc_info.commandPool = m_main_command_pool;

    // primary buffers can be submitted to a queue for execution but cannot be called from other command buffers
    primary_alloc_info.level = vk::CommandBufferLevel::ePrimary;
    primary_alloc_info.commandBufferCount = 1;

    vk::CommandBufferAllocateInfo extra_alloc_info{};
    extra_alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
    extra_alloc_info.commandPool = m_extra_command_pool;
    extra_alloc_info.level = vk::CommandBufferLevel::eSecondary;
    extra_alloc_info.commandBufferCount = 1;

    vk::CommandBufferAllocateInfo imgui_alloc_info{};
    imgui_alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
    imgui_alloc_info.commandPool = m_main_command_pool;
    imgui_alloc_info.level = vk::CommandBufferLevel::eSecondary;
    imgui_alloc_info.commandBufferCount = 1;

	for(u32 i = 0; i < s_max_frames_in_flight; ++i)
	{
		if (logical_device.allocateCommandBuffers(&primary_alloc_info, &m_primary_command_buffers[i].vk_command_buffer) != vk::Result::eSuccess ||
        logical_device.allocateCommandBuffers(&extra_alloc_info, &m_extra_draw_commands[i].vk_command_buffer) != vk::Result::eSuccess ||
        logical_device.allocateCommandBuffers(&imgui_alloc_info, &m_imgui_commands[i].vk_command_buffer) != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}
}

void Renderer::init_framebuffers()
{
    m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

    for(size_t i = 0; i < m_swapchain_image_views.size(); ++i)
    {
        std::array<vk::ImageView, 2> attachments = { m_swapchain_image_views[i], m_depth_image_view };

        // you can only use a framebuffer with a compatible render pass
        // meaning they use the same number and type of attachments
        vk::FramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = vk::StructureType::eFramebufferCreateInfo;
        framebuffer_info.renderPass = m_render_pass;
        framebuffer_info.attachmentCount = static_cast<unsigned>(attachments.size());
        framebuffer_info.pAttachments = attachments.data();
        framebuffer_info.width = m_swapchain_extent.width;
        framebuffer_info.height = m_swapchain_extent.height;
        framebuffer_info.layers = 1;

        if(logical_device.createFramebuffer(&framebuffer_info, nullptr, &m_swapchain_framebuffers[i]) != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void Renderer::init_descriptor_pools()
{
    // first describe which descriptor types our descriptor sets use and how many
    // FIXME: magic numbers
    vk::DescriptorPoolSize pool_sizes[] =
    {
        { vk::DescriptorType::eUniformBuffer, k_max_bindless_resources },
        { vk::DescriptorType::eCombinedImageSampler, k_max_bindless_resources },
        { vk::DescriptorType::eStorageImage, k_max_bindless_resources }
    };

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eUpdateAfterBindEXT; // for bindless resources
    pool_info.maxSets = 20;
    pool_info.poolSizeCount = 3;
    pool_info.pPoolSizes = pool_sizes;

    if(logical_device.createDescriptorPool(&pool_info, nullptr, &m_descriptor_pool) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void Renderer::init_depth_resources()
{
    create_image(m_swapchain_extent.width, m_swapchain_extent.height,
                 vk::Format::eD32Sfloat, vk::ImageTiling::eOptimal,
                 vk::ImageUsageFlagBits::eDepthStencilAttachment,
                 vk::MemoryPropertyFlagBits::eDeviceLocal,
                 m_depth_image, m_depth_image_memory);

    m_depth_image_view = create_image_view(m_depth_image, vk::Format::eD32Sfloat, vk::ImageAspectFlagBits::eDepth);
}

void Renderer::init_sync_objects()
{
    vk::SemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = vk::StructureType::eSemaphoreCreateInfo;

    vk::FenceCreateInfo fence_info{};
    fence_info.sType = vk::StructureType::eFenceCreateInfo;

    // need it to start as signaled so wait for fence will return immediately on the first frame
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    for(size_t i = 0; i < s_max_frames_in_flight; ++i)
    {
        if(logical_device.createSemaphore(&semaphore_info, nullptr, &m_image_available_semaphores[i]) != vk::Result::eSuccess ||
           logical_device.createSemaphore(&semaphore_info, nullptr, &m_render_finished_semaphores[i]) != vk::Result::eSuccess ||
           logical_device.createFence(&fence_info, nullptr, &m_in_flight_fences[i]) != vk::Result::eSuccess
                )
        {
            throw std::runtime_error("failed to create sync objects!");
        }
    }
}

void Renderer::cleanup_swapchain()
{
    for(auto framebuffer : m_swapchain_framebuffers)
    {
        logical_device.destroyFramebuffer(framebuffer, nullptr);
    }

    for(auto image_view : m_swapchain_image_views)
    {
        logical_device.destroyImageView(image_view, nullptr);
    }

    logical_device.destroyImage(m_depth_image, nullptr);
    logical_device.freeMemory(m_depth_image_memory);
    logical_device.destroyImageView(m_depth_image_view, nullptr);

    logical_device.destroySwapchainKHR(m_swapchain, nullptr);
}

void Renderer::init_imgui()
{
    vk::DescriptorPoolSize pool_sizes[] =
    {
            { vk::DescriptorType::eSampler, 1000 },
            { vk::DescriptorType::eCombinedImageSampler, 1000 },
            { vk::DescriptorType::eSampledImage, 1000 },
            { vk::DescriptorType::eStorageImage, 1000 },
            { vk::DescriptorType::eUniformTexelBuffer, 1000 },
            { vk::DescriptorType::eStorageTexelBuffer, 1000 },
            { vk::DescriptorType::eUniformBuffer, 1000 },
            { vk::DescriptorType::eStorageBuffer, 1000 },
            { vk::DescriptorType::eUniformBufferDynamic, 1000 },
            { vk::DescriptorType::eStorageBufferDynamic, 1000 },
            { vk::DescriptorType::eInputAttachment, 1000 }
    };

    vk::DescriptorPoolCreateInfo pool_info{};
    pool_info.sType = vk::StructureType::eDescriptorPoolCreateInfo;
    pool_info.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    if(logical_device.createDescriptorPool(&pool_info, nullptr, &m_imgui_pool) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to create imgui descriptor pool!");
    }

    ImGui::CreateContext();

    ImGui_ImplGlfw_InitForVulkan(m_window, true);

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = m_instance;
    init_info.PhysicalDevice = m_physical_device;
    init_info.Device = logical_device;
    init_info.Queue = m_graphics_queue;
    init_info.DescriptorPool = m_imgui_pool;
    init_info.MinImageCount = s_max_frames_in_flight;
    init_info.ImageCount = s_max_frames_in_flight;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, m_render_pass);

    vk::CommandBuffer upload_fonts = begin_single_time_commands();
    ImGui_ImplVulkan_CreateFontsTexture(upload_fonts);
    end_single_time_commands(upload_fonts);

    //clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();
}

// memory transfer ops are executed with command buffers
void Renderer::copy_buffer(vk::Buffer src_buffer, vk::Buffer dst_buffer, vk::DeviceSize size)
{
    vk::CommandBuffer command_buffer = begin_single_time_commands();

    vk::BufferCopy copy_region{};
    copy_region.srcOffset = 0; // optional
    copy_region.dstOffset = 0; // optional
    copy_region.size = size;
    command_buffer.copyBuffer(src_buffer, dst_buffer, 1, &copy_region);

    end_single_time_commands(command_buffer);
}

void Renderer::copy_buffer_to_image(vk::Buffer buffer, vk::Image image, u32 width, u32 height)
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

void Renderer::transition_image_layout(vk::Image image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout)
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

size_t Renderer::pad_uniform_buffer(size_t original_size) const
{
    size_t alignment = m_device_properties.limits.minUniformBufferOffsetAlignment;
    size_t aligned_size = (alignment + original_size - 1) & ~(alignment - 1);
    return aligned_size;
}

vk::CommandBuffer Renderer::begin_single_time_commands()
{
    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = vk::StructureType::eCommandBufferAllocateInfo;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandPool = m_command_pools[0];
    alloc_info.commandBufferCount = 1;

    vk::CommandBuffer command_buffer;
    if(logical_device.allocateCommandBuffers(&alloc_info, &command_buffer) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to allocate command buffer for single time use!");
    }

    // only using the command buffer once and wait with until the copy is finished
    // its good practice to tell the driver about intend, hence one time submit flag
    vk::CommandBufferBeginInfo begin_info{};
    begin_info.sType = vk::StructureType::eCommandBufferBeginInfo;
    begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

    if(command_buffer.begin(&begin_info) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to begin command buffer!");
    }

    return command_buffer;
}

void Renderer::end_single_time_commands(vk::CommandBuffer command_buffer)
{
    command_buffer.end();

    vk::SubmitInfo  submit_info{};
    submit_info.sType = vk::StructureType::eSubmitInfo;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    if(m_graphics_queue.submit(1, &submit_info, nullptr) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to submit to graphics queue!");
    }
    m_graphics_queue.waitIdle();

    logical_device.freeCommandBuffers(m_command_pools[0], 1, & command_buffer);
}

bool Renderer::check_validation_layer_support()
{
    uint32_t layer_count;

    if(vk::enumerateInstanceLayerProperties(&layer_count, nullptr) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to enumerate instance properties!");
    }

    std::vector<vk::LayerProperties> available_layers(layer_count);

    if(vk::enumerateInstanceLayerProperties(&layer_count, available_layers.data()) != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to enumerate instance properties!");
    }

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

[[nodiscard]] std::vector<const char*> Renderer::get_required_extensions() const
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