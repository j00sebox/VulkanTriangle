#include "Engine.hpp"

Engine::Engine(int width, int height, GLFWwindow* window) :
    m_width(width),
    m_height(height)
{
    create_instance();
    create_surface();
}

Engine::~Engine()
{
    if (m_enable_validation_layers)
    {
        m_instance.destroyDebugUtilsMessengerEXT(m_debug_messenger, nullptr, m_dispatch_loader);
    }
    m_instance.destroySurfaceKHR(m_surface, nullptr);
    m_instance.destroy();
}

void Engine::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwPollEvents();
    }
}

void Engine::create_instance()
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
    app_info.apiVersion = VK_API_VERSION_1_1;

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

void Engine::create_surface()
{
    VkSurfaceKHR c_style_surface;
    if (glfwCreateWindowSurface(m_instance, m_window, nullptr, &c_style_surface) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create window surface!");
    }

    m_surface = c_style_surface;
}

bool Engine::check_validation_layer_support()
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

[[nodiscard]] std::vector<const char*> Engine::get_required_extensions() const
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

static void framebuffer_resize_callback(GLFWwindow* window, int width, int height)
{
    auto engine = reinterpret_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine->m_framebuffer_resized = true;
}