#pragma once

#include "config.hpp"
#include <vulkan/vulkan.hpp>

struct CommandBuffer
{
	void begin();
	void begin(vk::CommandBufferInheritanceInfo inheritance_info);
	void bind_pipeline(vk::Pipeline pipeline);
	void set_viewport(u32 width, u32 height);
	void set_scissor(vk::Extent2D extent);
	void end();

	vk::CommandBuffer vk_command_buffer;

private:
	bool m_is_recording = false;
};
