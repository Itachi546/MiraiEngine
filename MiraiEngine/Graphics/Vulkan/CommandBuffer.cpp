#include "CommandBuffer.hpp"
#include "VulkanRenderingDevice.hpp"
#include "Swapchain.h"
#include "VulkanUtils.hpp"
#include "Scene/FrameGraph.hpp"
namespace mirai
{
    VkImageLayout find_required_barrier_info(bool is_depth_texture, VkImageAspectFlags image_aspect, FrameGraphResourceType resource_type, VkAccessFlags &access_flag)
    {
        VkImageLayout required_layout;
        bool is_attachment = resource_type == FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT;
        if (!is_attachment)
        {
            required_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            access_flag = VK_ACCESS_SHADER_READ_BIT;
        }
        else
        {
            if (is_depth_texture)
            {
                if (image_aspect & VK_IMAGE_ASPECT_STENCIL_BIT)
                    required_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                else
                    required_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

                access_flag = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
            }
            else
            {
                required_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

                access_flag = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            }
        }
        return required_layout;
    }

    void create_image_barrier(VulkanRenderingDevice *device, FrameGraphResource *resource,
                              std::vector<VkImageMemoryBarrier> &color_barriers,
                              std::vector<VkImageMemoryBarrier> &depth_barriers)
    {
        VulkanTexture *texture = device->access_texture(resource->texture);
        bool is_depth_texture = (texture->image_aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0;

        VkAccessFlags access_mask = 0;
        VkImageLayout required_layout = find_required_barrier_info(is_depth_texture, texture->image_aspect, resource->resource_type, access_mask);

        if (texture->current_layout == required_layout)
            return;

        if (is_depth_texture)
        {
            depth_barriers.push_back(CreateImageMemoryBarrier(texture->image,
                                                              texture->image_aspect, 0,
                                                              access_mask,
                                                              texture->current_layout,
                                                              required_layout));
            texture->current_layout = required_layout;
        }
        else
        {
            color_barriers.push_back(CreateImageMemoryBarrier(texture->image,
                                                              texture->image_aspect,
                                                              0, access_mask,
                                                              texture->current_layout,
                                                              required_layout));

            texture->current_layout = required_layout;
        }
    }

    CommandBuffer::CommandBuffer()
    {
        device = static_cast<VulkanRenderingDevice *>(RenderingDevice::get());
    }

    void CommandBuffer::begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph)
    {
        prepare_render_pass_resources(frame_graph, node);

        uint32_t attachment_count = static_cast<uint32_t>(node->outputs.size());

        std::vector<VkRenderingAttachmentInfo> color_attachments;
        std::optional<VkRenderingAttachmentInfo> depth_attachment;
        bool has_stencil_attachment = false;

        const FrameGraphRenderingInfo &frame_graph_rendering_info = node->rendering_info;
        for (uint32_t i = 0; i < frame_graph_rendering_info.attachment_info.size(); ++i)
        {
            const FrameGraphAttachmentInfo *attachment = &frame_graph_rendering_info.attachment_info[i];
            FrameGraphResource *resource = frame_graph->get_resource(node->outputs[i]);

            VkRenderingAttachmentInfo attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            attachment_info.loadOp = VkAttachmentLoadOp(attachment->load_op);
            attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

            if (i == frame_graph_rendering_info.depth_attachment_index)
            {
                attachment_info.clearValue.depthStencil = {attachment->clear_color.r, 0};
                attachment_info.imageLayout = frame_graph_rendering_info.has_stencil_attachment ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                attachment_info.imageView = device->access_texture(resource->texture)->image_view;
                depth_attachment = std::move(attachment_info);
            }
            else
            {
                if (resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
                {
                    VulkanSwapchain *swapchain = device->get_swapchain();
                    attachment_info.imageView = swapchain->get_current_image_view();
                }
                else
                {
                    attachment_info.imageView = device->access_texture(resource->texture)->image_view;
                }
                attachment_info.clearValue = {
                    attachment->clear_color.r,
                    attachment->clear_color.g,
                    attachment->clear_color.b,
                    attachment->clear_color.a,
                };
                attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                color_attachments.push_back(std::move(attachment_info));
            }
        }

        uint32_t width = node->renderer->get_width();
        uint32_t height = node->renderer->get_height();
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

        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(width),
            .height = static_cast<float>(height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        vkCmdSetViewport(command_buffer, 0, 1, &viewport);

        VkRect2D scissor{{0, 0}, {width, height}};
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }

    void CommandBuffer::bind_pipeline(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count, PushConstant *push_constants, uint32_t push_constant_count)
    {
        ASSERT(pipeline_id.is_valid());
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        vkCmdBindPipeline(command_buffer, pipeline->bind_point, pipeline->pipeline);

        uint32_t thread_id = 1;

        std::vector<VkDescriptorSet> descriptor_sets(uniform_set_count);
        for (uint32_t i = 0; i < uniform_set_count; ++i)
        {
            VulkanUniformSet *uniform_set = device->access_uniform_set(uniform_sets[i]);
            vkCmdBindDescriptorSets(command_buffer,
                                    pipeline->bind_point, pipeline->pipeline_layout,
                                    uniform_set->set_id,
                                    1, &uniform_set->descriptor_set,
                                    0, nullptr);
        }

        for (uint32_t i = 0; i < push_constant_count; ++i)
        {
            PushConstant *push_constant = &push_constants[i];
            vkCmdPushConstants(command_buffer, pipeline->pipeline_layout,
                               VkShaderStageFlags(push_constant->shader_stage),
                               push_constant->offset, push_constant->size, push_constant->data);
        }
    }

    void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance)
    {
        vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    void CommandBuffer::draw_indexed_indirect(BufferID buffer, uint32_t offset, uint32_t draw_count, uint32_t stride)
    {
        VulkanBuffer *indirect_buffer = device->access_buffer(buffer);
        vkCmdDrawIndexedIndirect(command_buffer, indirect_buffer->buffer, offset, draw_count, stride);
    }

    void CommandBuffer::set_index_buffer(BufferID buffer)
    {
        VulkanBuffer *index_buffer = device->access_buffer(buffer);
        vkCmdBindIndexBuffer(command_buffer, index_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void CommandBuffer::copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion &region)
    {
        VulkanBuffer *src_buffer = device->access_buffer(src);
        VulkanBuffer *dst_buffer = device->access_buffer(dst);

        ASSERT((region.src_offset + region.size) <= src_buffer->size);
        ASSERT((region.dst_offset + region.size) <= dst_buffer->size);
        vkCmdCopyBuffer(command_buffer, src_buffer->buffer, dst_buffer->buffer, 1, (const VkBufferCopy *)&region);
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

    void CommandBuffer::prepare_render_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node)
    {

        std::vector<VkImageMemoryBarrier> color_image_barriers;
        std::vector<VkImageMemoryBarrier> depth_image_barriers;

        for (auto resource_handle : node->inputs)
        {
            FrameGraphResource *resource = frame_graph->get_resource(resource_handle);
            create_image_barrier(device, resource, color_image_barriers, depth_image_barriers);
        }
        if (color_image_barriers.size() > 0)
        {
            // @TODO setup barrier properly, esp top of pipe bit
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<uint32_t>(color_image_barriers.size()), color_image_barriers.data());
            color_image_barriers.clear();
        }

        if (depth_image_barriers.size() > 0)
        {
            // @TODO setup barrier properly, esp top of pipe bit
            vkCmdPipelineBarrier(command_buffer,
                                 VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                 VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                 VK_DEPENDENCY_BY_REGION_BIT,
                                 0, nullptr,
                                 0, nullptr,
                                 static_cast<uint32_t>(depth_image_barriers.size()), depth_image_barriers.data());
            depth_image_barriers.clear();
        }

        for (auto resource_handle : node->outputs)
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
                create_image_barrier(device, resource, color_image_barriers, depth_image_barriers);
            }
        }

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