#pragma once

#include "config.hpp"
#include <vulkan/vulkan.hpp>

struct CommandBuffer
{
	void begin();
	void begin(vk::CommandBufferInheritanceInfo inheritance_info);
    void begin_renderpass(const vk::RenderPass& renderpass, const vk::Framebuffer& framebuffer, vk::Extent2D swapchain_extent, vk::SubpassContents subpass_contents) const;
	void bind_pipeline(const vk::Pipeline& pipeline) const;
	void set_viewport(u32 width, u32 height) const;
	void set_scissor(vk::Extent2D extent) const;
    void end_renderpass() const;
	void end();

	vk::CommandBuffer vk_command_buffer;

private:
	bool m_is_recording = false;
};
