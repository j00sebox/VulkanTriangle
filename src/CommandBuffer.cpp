#include "CommandBuffer.hpp"

void CommandBuffer::begin()
{
	vk_command_buffer.reset();
	vk::CommandBufferBeginInfo begin_info{};
	begin_info.sType = vk::StructureType::eCommandBufferBeginInfo;
	begin_info.pInheritanceInfo = nullptr; // only relevant to secondary command buffers

	if (vk_command_buffer.begin(&begin_info)!= vk::Result::eSuccess)
    {
        throw std::runtime_error("failed to begin recording of framebuffer!");
    }
	m_is_recording = true;
}

void CommandBuffer::begin(vk::CommandBufferInheritanceInfo inheritance_info)
{
	vk_command_buffer.reset();

	vk::CommandBufferBeginInfo secondary_begin_info{};
	secondary_begin_info.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue | vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
	secondary_begin_info.pInheritanceInfo = &inheritance_info;

	vk_command_buffer.begin(secondary_begin_info);

	m_is_recording = true;
}

void CommandBuffer::begin_renderpass(vk::RenderPassBeginInfo renderpass_begin_info, vk::SubpassContents subpass_contents)
{
    vk_command_buffer.beginRenderPass(&renderpass_begin_info, subpass_contents);
}

void CommandBuffer::bind_pipeline(vk::Pipeline pipeline)
{
	vk_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
}

void CommandBuffer::set_viewport(u32 width, u32 height)
{
	vk::Viewport viewport{};
	viewport.x = 0.f;
	viewport.y = 0.f;
	viewport.width = static_cast<float>(width);
	viewport.height = static_cast<float>(height);
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;
	vk_command_buffer.setViewport(0, 1, &viewport);
}

void CommandBuffer::set_scissor(vk::Extent2D extent)
{
	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{0, 0};
	scissor.extent = extent;
	vk_command_buffer.setScissor(0, 1, &scissor);
}

void CommandBuffer::end()
{
	if(m_is_recording)
	{
		vk_command_buffer.end();
		m_is_recording = false;
	}
}