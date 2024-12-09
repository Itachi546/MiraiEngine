#include "CommandBuffer.hpp"
#include "VulkanRenderingDevice.hpp"
#include "Swapchain.h"
#include "VulkanUtils.hpp"
#include "Scene/FrameGraph.hpp"

namespace mirai {

    CommandBuffer::CommandBuffer() {
        device = static_cast<VulkanRenderingDevice *>(RenderingDevice::get());
    }

    void CommandBuffer::begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph) {
        prepare_render_pass_resources(frame_graph, node);

        std::vector<VkRenderingAttachmentInfo> color_attachments;
        std::optional<VkRenderingAttachmentInfo> depth_attachment;
        bool has_stencil_attachment = false;

        const FrameGraphRenderpassInfo &renderpass = node->renderpass_info;
        uint32_t width = node->width;
        uint32_t height = node->height;

        for (uint32_t i = 0; i < renderpass.attachment_info.size(); ++i) {
            const FrameGraphAttachmentInfo *attachment = &renderpass.attachment_info[i];
            const TextureID texture_id = attachment->texture;

            VkImageView image_view = VK_NULL_HANDLE;
            ASSERT(texture_id.is_valid());
            if (texture_id == K_SWAPCHAIN_TEXTURE_HANDLE) {
                VulkanSwapchain *swapchain = device->get_swapchain();
                image_view = swapchain->get_current_image_view();
                width = swapchain->width;
                height = swapchain->height;
            } else
                image_view = device->access_texture(texture_id)->image_view;

            VkRenderingAttachmentInfo attachment_info = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            attachment_info.loadOp = VkAttachmentLoadOp(attachment->load_op);
            attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            if (i == renderpass.depth_attachment_index) {
                attachment_info.clearValue.depthStencil = {attachment->clear_color.r, 0};
                attachment_info.imageLayout = renderpass.has_stencil_attachment ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
                attachment_info.imageView = image_view;
                depth_attachment = std::move(attachment_info);
            } else {
                attachment_info.imageView = image_view;
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

    void CommandBuffer::bind_pipeline(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count, PushConstant *push_constants, uint32_t push_constant_count) {
        ASSERT(pipeline_id.is_valid());
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        vkCmdBindPipeline(command_buffer, pipeline->bind_point, pipeline->pipeline);

        set_uniform_sets(pipeline_id, uniform_sets, uniform_set_count);
        set_push_constants(pipeline_id, push_constants, push_constant_count);

        if (pipeline->support_bindless_texture)
            vkCmdBindDescriptorSets(command_buffer, pipeline->bind_point, pipeline->pipeline_layout, K_BINDLESS_TEXTURE_SET, 1, &device->bindless_descriptor_set, 0, nullptr);
    }

    void CommandBuffer::set_uniform_sets(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count) {
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        std::vector<VkDescriptorSet> descriptor_sets(uniform_set_count);
        for (uint32_t i = 0; i < uniform_set_count; ++i) {
            VulkanUniformSet *uniform_set = device->access_uniform_set(uniform_sets[i]);
            vkCmdBindDescriptorSets(command_buffer,
                                    pipeline->bind_point, pipeline->pipeline_layout,
                                    uniform_set->set_id,
                                    1, &uniform_set->descriptor_set,
                                    0, nullptr);
        }
    }

    void CommandBuffer::set_push_constants(PipelineID pipeline_id, PushConstant *push_constants, uint32_t push_constant_count) {
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        for (uint32_t i = 0; i < push_constant_count; ++i) {
            PushConstant *push_constant = &push_constants[i];
            vkCmdPushConstants(command_buffer, pipeline->pipeline_layout,
                               VkShaderStageFlags(push_constant->shader_stage),
                               push_constant->offset, push_constant->size, push_constant->data);
        }
    }

    void CommandBuffer::draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance) {
        vkCmdDraw(command_buffer, vertex_count, instance_count, first_vertex, first_instance);
    }

    void CommandBuffer::draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance) {
        vkCmdDrawIndexed(command_buffer, index_count, instance_count, first_index, vertex_offset, first_instance);
    }

    void CommandBuffer::draw_indexed_indirect(BufferID buffer, uint32_t offset, uint32_t draw_count, uint32_t stride) {
        VulkanBuffer *indirect_buffer = device->access_buffer(buffer);
        vkCmdDrawIndexedIndirect(command_buffer, indirect_buffer->buffer, offset, draw_count, stride);
    }

    void CommandBuffer::set_vertex_buffer(BufferID buffer) {
        VulkanBuffer *vertex_buffer = device->access_buffer(buffer);

        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer->buffer, &offset);
    }

    void CommandBuffer::set_index_buffer(BufferID buffer) {
        VulkanBuffer *index_buffer = device->access_buffer(buffer);
        vkCmdBindIndexBuffer(command_buffer, index_buffer->buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    void CommandBuffer::copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion &region) {
        VulkanBuffer *src_buffer = device->access_buffer(src);
        VulkanBuffer *dst_buffer = device->access_buffer(dst);

        ASSERT((region.src_offset + region.size) <= src_buffer->size);
        ASSERT((region.dst_offset + region.size) <= dst_buffer->size);
        vkCmdCopyBuffer(command_buffer, src_buffer->buffer, dst_buffer->buffer, 1, (const VkBufferCopy *)&region);
    }

    void CommandBuffer::copy_texture(TextureID dst, BufferID src, uint32_t buffer_offset, uint32_t mip_count, uint32_t block_size) {
        VulkanBuffer *src_buffer = device->access_buffer(src);
        VulkanTexture *dst_image = device->access_texture(dst);

        VkImageMemoryBarrier2 transfer_dst_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = 0,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image->image,
            .subresourceRange = {
                .aspectMask = dst_image->image_aspect,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        pipeline_barrier(&transfer_dst_barrier, 1);

        uint32_t mip_width = dst_image->width;
        uint32_t mip_height = dst_image->height;

        for (uint32_t mip = 0; mip < mip_count; ++mip) {
            VkBufferImageCopy copy_region = {
                .bufferOffset = buffer_offset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource = {
                    .aspectMask = dst_image->image_aspect,
                    .mipLevel = mip,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
                .imageOffset = {0, 0, 0},
                .imageExtent = {
                    .width = mip_width,
                    .height = mip_height,
                    .depth = 1,
                },
            };

            vkCmdCopyBufferToImage(command_buffer, src_buffer->buffer, dst_image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            buffer_offset += ((mip_width + 3) / 4) * ((mip_height + 3) / 4) * block_size;
            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
        }

        VkImageMemoryBarrier2 shader_read_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = dst_image->image,
            .subresourceRange = {
                .aspectMask = dst_image->image_aspect,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        pipeline_barrier(&shader_read_barrier, 1);
        dst_image->current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    void CommandBuffer::end_render_pass() {
        vkCmdEndRendering(command_buffer);
    }

    void CommandBuffer::begin() {
        VkCommandBufferBeginInfo begin_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));
    }

    void CommandBuffer::wait() {
        VK_CHECK(vkWaitForFences(device->device, 1, &fence, VK_TRUE, UINT64_MAX));
        vkResetFences(device->device, 1, &fence);
    }

    void CommandBuffer::prepare_render_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node) {
        const std::vector<FrameGraphResourceState> &resources_state = node->resources_state;
        std::vector<VkImageMemoryBarrier2> image_barriers;
        for (auto &state : resources_state) {
            FrameGraphResource *resource = frame_graph->get_resource(state.resource_handle);
            if (resource->handle == K_SWAPCHAIN_TEXTURE_HANDLE) {
                prepare_swapchain_image(&state, image_barriers);
            } else {
                VulkanTexture *texture = device->access_texture(resource->handle);
                VkPipelineStageFlags2 src_stage_mask = VkPipelineStageFlags2(texture->stage_mask);

                // This is the special case for depth when the last stage is not same as current previous stage
                if (src_stage_mask == VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
                    src_stage_mask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;

                image_barriers.push_back(CreateImageMemoryBarrier2(texture->image,
                                                                   src_stage_mask, texture->access_flags,
                                                                   VkPipelineStageFlags2(state.stage_mask), VkAccessFlags2(state.access_flags),
                                                                   texture->current_layout, VkImageLayout(state.layout),
                                                                   texture->image_aspect));
                texture->current_layout = VkImageLayout(state.layout);
                texture->access_flags = VkAccessFlags2(state.access_flags);
                texture->stage_mask = VkPipelineStageFlags2(state.stage_mask);
            }
        }
        pipeline_barrier(image_barriers.data(), static_cast<uint32_t>(image_barriers.size()));
    }

    void CommandBuffer::prepare_swapchain_image(const FrameGraphResourceState *state, std::vector<VkImageMemoryBarrier2> &image_barriers) {
        VulkanSwapchain *swapchain = device->get_swapchain();
        VkImageLayout current_layout = swapchain->get_current_image_layout();
        if (current_layout != VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
            image_barriers.push_back(CreateImageMemoryBarrier2(swapchain->get_current_image(),
                                                               VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, 0,
                                                               VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, state->access_flags,
                                                               current_layout,
                                                               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                               VK_IMAGE_ASPECT_COLOR_BIT));
            swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        }
    }

    void CommandBuffer::pipeline_barrier(VkImageMemoryBarrier2 *image_memory_barriers, uint32_t image_memory_barrier_count) {
        VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = 0,
            .pBufferMemoryBarriers = nullptr,
            .imageMemoryBarrierCount = image_memory_barrier_count,
            .pImageMemoryBarriers = image_memory_barriers,
        };
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }

} // namespace mirai