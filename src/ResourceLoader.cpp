#include "config.hpp"
#include "ResourceLoader.hpp"

ResourceLoader::ResourceLoader(Renderer* renderer, enki::TaskScheduler* scheduler) :
    m_renderer(renderer),
    m_scheduler(scheduler)
{
    // FIXME: magic numbers
    m_transfer_buffer = m_renderer->create_buffer({
        .usage = vk::BufferUsageFlagBits::eTransferSrc,
        .size = 64 * 1024 * 1024, // 64MB
        .persistent = true
    });

    vk::CommandPoolCreateInfo pool_info{};
    pool_info.sType = vk::StructureType::eCommandPoolCreateInfo;

    // allows command buffers to be rerecorded individually
    // without this flag they all have to be reset together
    // we will be recording a command buffer every frame so we want to be able to write to it
    pool_info.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    if(m_renderer->logical_device.createCommandPool(&pool_info, nullptr, &m_transfer_command_pool) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create command pool!");
    }

    vk::CommandBufferAllocateInfo alloc_info{};
    alloc_info.commandPool = m_transfer_command_pool;
    alloc_info.commandBufferCount = static_cast<u32>(m_transfer_command_buffers.size());

    if(m_renderer->logical_device.allocateCommandBuffers(&alloc_info, m_transfer_command_buffers.data()) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    vk::SemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = vk::StructureType::eSemaphoreCreateInfo;

    if(m_renderer->logical_device.createSemaphore(&semaphore_info, nullptr, &m_transfer_semaphore) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create transfer semaphore!");
    }

    vk::FenceCreateInfo fence_info{};
    fence_info.sType = vk::StructureType::eFenceCreateInfo;
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    if(m_renderer->logical_device.createFence(&fence_info, nullptr, &m_transfer_fence) != vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to create transfer fence!");
    }
}

ResourceLoader::~ResourceLoader()
{
    m_renderer->destroy_buffer(m_transfer_buffer);

    m_renderer->logical_device.destroySemaphore(m_transfer_semaphore, nullptr);
    m_renderer->logical_device.destroyFence(m_transfer_fence, nullptr);

    m_renderer->logical_device.destroyCommandPool(m_transfer_command_pool, nullptr);
}