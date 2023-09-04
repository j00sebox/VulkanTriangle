#pragma once

#include "Renderer.hpp"

#include <TaskScheduler.h>

class ResourceLoader
{
public:
    ResourceLoader(Renderer* renderer, enki::TaskScheduler* scheduler);
    ~ResourceLoader();

private:
    Renderer* m_renderer;
    enki::TaskScheduler* m_scheduler;

    // buffer that will be used by the resource loading thread
    u32 m_transfer_buffer;

    vk::CommandPool m_transfer_command_pool;
    std::array<vk::CommandBuffer, Renderer::s_max_frames_in_flight> m_transfer_command_buffers;
    vk::Semaphore m_transfer_semaphore;
    vk::Fence m_transfer_fence;
};


