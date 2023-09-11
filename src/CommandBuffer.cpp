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

void CommandBuffer::begin_renderpass(const vk::RenderPass& renderpass, const vk::Framebuffer& framebuffer, vk::Extent2D swapchain_extent, vk::SubpassContents subpass_contents) const
{
    vk::RenderPassBeginInfo renderpass_begin_info{};
    renderpass_begin_info.sType = vk::StructureType::eRenderPassBeginInfo;
    renderpass_begin_info.renderPass = renderpass;

    // need to bind the framebuffer for the swapchain image we want to draw to
    renderpass_begin_info.framebuffer = framebuffer;

    // define size of the render area;
    // render area defined as the place shader loads and stores happen
    renderpass_begin_info.renderArea.offset = vk::Offset2D{0, 0};
    renderpass_begin_info.renderArea.extent = swapchain_extent;

    std::array<vk::ClearValue, 2> clear_values{};
    vk::ClearColorValue value;
    value.float32[0] = 0.f; value.float32[1] = 0.f; value.float32[2] = 0.f; value.float32[3] = 1.f;
    clear_values[0].color = value;
    clear_values[1].depthStencil = vk::ClearDepthStencilValue{1.f, 0};

    renderpass_begin_info.clearValueCount = static_cast<unsigned>(clear_values.size());
    renderpass_begin_info.pClearValues = clear_values.data();

    vk_command_buffer.beginRenderPass(&renderpass_begin_info, subpass_contents);
}

void CommandBuffer::bind_pipeline(vk::Pipeline pipeline) const
{
	vk_command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
}

void CommandBuffer::set_viewport(u32 width, u32 height) const
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

void CommandBuffer::set_scissor(vk::Extent2D extent) const
{
	vk::Rect2D scissor{};
	scissor.offset = vk::Offset2D{0, 0};
	scissor.extent = extent;
	vk_command_buffer.setScissor(0, 1, &scissor);
}

void CommandBuffer::end_renderpass() const
{
    vk_command_buffer.endRenderPass();
}

void CommandBuffer::end()
{
	if(m_is_recording)
	{
		vk_command_buffer.end();
		m_is_recording = false;
	}
}