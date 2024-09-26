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
        prepare_render_pass_resources(node, frame_graph);

        uint32_t attachment_count = static_cast<uint32_t>(node->outputs.size());

        std::vector<VkRenderingAttachmentInfo> color_attachments;
        std::optional<VkRenderingAttachmentInfo> depth_attachment;
        bool has_stencil_attachment = false;

        for (uint32_t i = 0; i < attachment_count; ++i)
        {
            FrameGraphResource *resource = frame_graph->get_resource(node->outputs[i]);

            VkRenderingAttachmentInfo attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            attachment_info.loadOp = VkAttachmentLoadOp(resource->load_op);
            attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            if (resource->is_depth_texture)
            {
                attachment_info.clearValue.depthStencil = {resource->clear_colors.r, 0};
                has_stencil_attachment = is_stencil_format(resource->format);
                attachment_info.imageLayout = has_stencil_attachment ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                attachment_info.imageView = device->access_texture(resource->texture)->image_view;
                depth_attachment = std::move(attachment_info);
            }
            else
            {
                attachment_info.clearValue.color = {resource->clear_colors.r, resource->clear_colors.g, resource->clear_colors.b, resource->clear_colors.a};
                attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                if (resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
                {
                    VulkanSwapchain *swapchain = device->get_swapchain();
                    attachment_info.imageView = swapchain->get_current_image_view();
                }
                else
                {
                    attachment_info.imageView = device->access_texture(resource->texture)->image_view;
                }

                color_attachments.push_back(std::move(attachment_info));
            }
        }

        uint32_t width = node->width;
        uint32_t height = node->height;

        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {0, 0, width, height},
            .layerCount = 1,
            .colorAttachmentCount = static_cast<uint32_t>(color_attachments.size()),
            .pColorAttachments = color_attachments.data(),
            .pDepthAttachment = depth_attachment.has_value() ? &depth_attachment.value() : nullptr,
            .pStencilAttachment = has_stencil_attachment ? &depth_attachment.value() : nullptr,
        };

        vkCmdBeginRendering(command_buffer, &rendering_info);

        VkViewport viewport{0, 0, static_cast<float>(width), static_cast<float>(height)};
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{{0, 0}, {width, height}};
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

    void CommandBuffer::prepare_render_pass_resources(FrameGraphNode *node, FrameGraph *frame_graph)
    {

        std::vector<VkImageMemoryBarrier> color_image_barriers;
        std::vector<VkImageMemoryBarrier> depth_image_barriers;

        for (uint32_t i = 0; i < node->inputs.size(); ++i)
        {
            ASSERT_MSG(0, "Not Implemented !!!");
        }

        for (auto &resource_handle : node->outputs)
        {
            FrameGraphResource *resource = frame_graph->get_resource(resource_handle);
            if (resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
            {
                VulkanSwapchain *swapchain = device->get_swapchain();

                VkImageLayout current_layout = swapchain->get_current_image_layout();
                if (current_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                {
                    color_image_barriers.push_back(CreateImageMemoryBarrier(swapchain->get_current_image(),
                                                                            VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                                            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                                                                            current_layout,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));
                    swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                }
            }
            else
            {
                VulkanTexture *texture = device->access_texture(resource->texture);

                if (resource->is_depth_texture)
                {
                    VkImageLayout required_layout;
                    if (texture->image_aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
                        required_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    else
                        required_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                    if (texture->current_layout == required_layout)
                        continue;

                    depth_image_barriers.push_back(CreateImageMemoryBarrier(texture->image,
                                                                            texture->image_aspect, 0,
                                                                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                                            texture->current_layout,
                                                                            required_layout));
                    texture->current_layout = required_layout;
                }
                else
                {
                    if (texture->current_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                        continue;

                    color_image_barriers.push_back(CreateImageMemoryBarrier(texture->image,
                                                                            VK_IMAGE_ASPECT_COLOR_BIT, 0,
                                                                            texture->image_aspect,
                                                                            texture->current_layout,
                                                                            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL));

                    texture->current_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                }
            }
        };

        if (color_image_barriers.size() > 0)
        {
            // @TODO setup barrier properly, esp top of pipe bit
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<uint32_t>(color_image_barriers.size()), color_image_barriers.data());
        }

        if (depth_image_barriers.size() > 0)
        {
            // @TODO setup barrier properly, esp top of pipe bit
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<uint32_t>(depth_image_barriers.size()), depth_image_barriers.data());
        }
    }

} // namespace mirai