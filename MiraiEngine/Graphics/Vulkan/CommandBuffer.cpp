#include "CommandBuffer.hpp"
#include "VulkanRenderingDevice.hpp"
#include "Swapchain.h"
#include "VulkanUtils.hpp"
#include "Scene/FrameGraph.hpp"
namespace mirai
{

    CommandBuffer::CommandBuffer()
    {
        device = static_cast<VulkanRenderingDevice *>(RenderingDevice::get());
    }

    void CommandBuffer::begin_render_pass(FrameGraphNode *node, FrameGraph *frame_graph)
    {
        RenderPass *render_pass = &node->render_pass;

        auto &attachments = render_pass->color_attachments;
        std::vector<VkRenderingAttachmentInfo> attachment_infos(attachments.size());
        std::vector<VkImageMemoryBarrier> image_barriers;

        for (uint32_t i = 0; i < attachments.size(); ++i)
        {
            AttachmentInfo &attachment = attachments[i];

            attachment_infos[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            attachment_infos[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            attachment_infos[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment_infos[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment_infos[i].clearValue = {attachment.clear_color.r, attachment.clear_color.g, attachment.clear_color.b, attachment.clear_color.a};

            if (attachment.attachment_type == ATTACHMENT_TYPE_SWAPCHAIN)
            {
                VulkanSwapchain *swapchain = device->get_swapchain();

                attachment_infos[i].imageView = swapchain->get_current_image_view();
                VkImageLayout current_layout = swapchain->get_current_image_layout();
                if (current_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                {
                    image_barriers.push_back(CreateImageMemoryBarrier(swapchain->get_current_image(), VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, current_layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
                    swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                }
            }
            else
            {
                ASSERT_MSG(0, "Not implemented yet");
            }
        }

        if (image_barriers.size() > 0)
        {
            // @TODO setup barrier properly, esp top of pipe bit
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<uint32_t>(image_barriers.size()), image_barriers.data());
        }

        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {0, 0, render_pass->width, render_pass->height},
            .layerCount = 1,
            .colorAttachmentCount = static_cast<uint32_t>(attachment_infos.size()),
            .pColorAttachments = attachment_infos.data(),
            .pDepthAttachment = nullptr,
            .pStencilAttachment = nullptr,
        };

        vkCmdBeginRendering(command_buffer, &rendering_info);

        VkViewport viewport{0, 0, static_cast<float>(render_pass->width), static_cast<float>(render_pass->height)};
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{{0, 0}, {render_pass->width, render_pass->height}};
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }

    void CommandBuffer::bind_pipeline(PipelineID pipeline_id)
    {
        ASSERT(pipeline_id.is_valid());
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        vkCmdBindPipeline(command_buffer, pipeline->bind_point, pipeline->pipeline);
    }

    void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
    {
        vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    void CommandBuffer::end_render_pass()
    {
        vkCmdEndRendering(command_buffer);
    }

    void CommandBuffer::begin()
    {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    }

} // namespace mirai