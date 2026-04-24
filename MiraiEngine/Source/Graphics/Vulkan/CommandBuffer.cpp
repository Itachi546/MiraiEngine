#include "CommandBuffer.hpp"
#include "VulkanRenderingDevice.hpp"
#include "VulkanSwapchain.hpp"
#include "VulkanUtils.hpp"

namespace mirai {

    CommandBuffer::CommandBuffer() {
        device = static_cast<VulkanRenderingDevice *>(RenderingDevice::get());
    }

    void CommandBuffer::begin_render_pass(const std::vector<AttachmentInfo> &color_attachments, const std::optional<AttachmentInfo> &depth_attachment, uint32_t render_area_width, uint32_t render_area_height) {
        uint32_t layer_count = 1;

        VkRenderingInfo rendering_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = {{0, 0}, {render_area_width, render_area_height}},
        };

        std::vector<VkRenderingAttachmentInfo> vk_color_attachments;
        for (uint32_t i = 0; i < color_attachments.size(); ++i) {
            const AttachmentInfo &attachment = color_attachments[i];
            VkImageView image_view = VK_NULL_HANDLE;
            uint32_t width = 0;
            uint32_t height = 0;

            if (attachment.texture == K_SWAPCHAIN_TEXTURE_HANDLE) {
                VulkanSwapchain *swapchain = device->get_swapchain();
                image_view = swapchain->get_current_image_view();
                width = swapchain->width;
                height = swapchain->height;
            } else {
                VulkanTexture *texture = device->access_texture(attachment.texture);
                image_view = texture->image_views[0];
                width = texture->width;
                height = texture->height;
                layer_count = std::max(texture->array_layers, layer_count);
            }
            vk_color_attachments.emplace_back(VkRenderingAttachmentInfo{
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = image_view,
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VkAttachmentLoadOp(attachment.load_op),
                .storeOp = VkAttachmentStoreOp(attachment.store_op),
                .clearValue = {
                    .color = {
                        .float32 = {
                            attachment.clear_color.r,
                            attachment.clear_color.g,
                            attachment.clear_color.b,
                            attachment.clear_color.a,
                        },
                    },
                },
            });
            ASSERT(render_area_width == width && render_area_height == height);
        }

        rendering_info.layerCount = layer_count;
        rendering_info.colorAttachmentCount = cast_u32(vk_color_attachments.size());
        rendering_info.pColorAttachments = vk_color_attachments.data();

        std::optional<VkRenderingAttachmentInfo> vk_depth_attachment;
        if (depth_attachment.has_value()) {
            VulkanTexture *texture = device->access_texture(depth_attachment->texture);
            bool has_stencil_attachment = is_stencil_format(texture->format);
            vk_depth_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .pNext = nullptr,
                .imageView = texture->image_views[0],
                .imageLayout = has_stencil_attachment ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                .loadOp = VkAttachmentLoadOp(depth_attachment->load_op),
                .storeOp = VkAttachmentStoreOp(depth_attachment->store_op),
                .clearValue = {
                    .depthStencil = {
                        depth_attachment->clear_color.r,
                        0,
                    }},
            };

            VkRenderingAttachmentInfo *p_depth_attachment = &vk_depth_attachment.value();
            rendering_info.pDepthAttachment = p_depth_attachment;
            rendering_info.pStencilAttachment = has_stencil_attachment ? p_depth_attachment : nullptr;
        }
        vkCmdBeginRendering(command_buffer, &rendering_info);
    }

    void CommandBuffer::set_viewport(const Viewport &viewport) {
        VkViewport vk_viewport = {
            .x = viewport.x,
            .y = viewport.y + viewport.height,
            .width = viewport.width,
            .height = -viewport.height,
            .minDepth = viewport.min_depth,
            .maxDepth = viewport.max_depth,
        };
        vkCmdSetViewport(command_buffer, 0, 1, &vk_viewport);
    }

    void CommandBuffer::set_scissor(int x, int y, uint32_t width, uint32_t height) {
        VkRect2D scissor = {
            .offset = {x, y},
            .extent = {width, height},
        };
        vkCmdSetScissor(command_buffer, 0, 1, &scissor);
    }

    void CommandBuffer::prepare_resources(const std::vector<ResourceAccessDeclaration> &resource_states) {
        if (resource_states.size() == 0)
            return;
        std::vector<VkImageMemoryBarrier2> image_barriers;
        std::vector<VkBufferMemoryBarrier2> memory_barriers;

        for (auto &state : resource_states) {
            switch (state.resource_type) {
            case ResourceType::Buffer: {
                VulkanBuffer *buffer = device->access_buffer(state.resource);
                VkPipelineStageFlags2 dst_stage = VkPipelineStageFlags2(state.declaration->stage_mask);
                VkAccessFlags2 dst_access_flag = VkAccessFlags2(state.declaration->access_flags);
                memory_barriers.push_back(CreateBufferMemoryBarrier2(
                    buffer->buffer,
                    buffer->stage_mask,
                    buffer->access_flags,
                    dst_stage,
                    dst_access_flag,
                    0,
                    buffer->size));

                buffer->access_flags = dst_access_flag;
                buffer->stage_mask = dst_stage;
                break;
            };
            case ResourceType::Texture: {
                VkAccessFlags2 dst_access_flag = VkAccessFlags2(state.declaration->access_flags);
                VkPipelineStageFlags2 dst_stage = VkPipelineStageFlags2(state.declaration->stage_mask);
                VkImageLayout dst_layout = VkImageLayout(state.declaration->layout);

                // Special handling of swapchain as color attachment
                if (state.resource.id == K_SWAPCHAIN_TEXTURE_HANDLE.id) {
                    ASSERT(dst_stage = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
                    VulkanSwapchain *swapchain = device->get_swapchain();
                    uint32_t current_index = swapchain->current_image_index;
                    VkPipelineStageFlags2 src_stage_mask = swapchain->stage_mask[current_index];
                    image_barriers.push_back(CreateImageMemoryBarrier2(swapchain->images[current_index],
                                                                       src_stage_mask,
                                                                       swapchain->access_flags[current_index],
                                                                       dst_stage,
                                                                       dst_access_flag,
                                                                       swapchain->current_layouts[current_index],
                                                                       dst_layout,
                                                                       VK_IMAGE_ASPECT_COLOR_BIT));
                    swapchain->current_layouts[current_index] = dst_layout;
                    swapchain->access_flags[current_index] = dst_access_flag;
                    swapchain->stage_mask[current_index] = dst_stage;
                } else {
                    VulkanTexture *texture = device->access_texture(state.resource);
                    bool is_depth_texture = is_depth_format(texture->format);
                    VkPipelineStageFlags2 src_stage_mask = texture->stage_mask;
                    // Special case for depth texture
                    if (is_depth_texture && src_stage_mask == VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT)
                        src_stage_mask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;

                    image_barriers.push_back(CreateImageMemoryBarrier2(texture->image,
                                                                       src_stage_mask,
                                                                       texture->access_flags,
                                                                       dst_stage,
                                                                       dst_access_flag,
                                                                       texture->current_layout,
                                                                       dst_layout,
                                                                       texture->image_aspect));
                    texture->current_layout = dst_layout;
                    texture->access_flags = dst_access_flag;
                    texture->stage_mask = dst_stage;
                }

                break;
            };
            default: {
                ASSERT_MSG(0, "Unknown resource type");
            }
            }
        }
        pipeline_barrier(image_barriers.data(), cast_u32(image_barriers.size()), memory_barriers.data(), cast_u32(memory_barriers.size()));
    }

    void CommandBuffer::bind_pipeline(PipelineID pipeline_id) {
        ASSERT(pipeline_id.is_valid());
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        vkCmdBindPipeline(command_buffer, pipeline->bind_point, pipeline->pipeline);
    }

    void CommandBuffer::bind_resource_heap(BufferID buffer) {
        VulkanBuffer *vk_buffer = device->access_buffer(buffer);
        VkBindHeapInfoEXT bind_info = {
            .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
            .pNext = nullptr,
            .heapRange = {
                .address = vk_buffer->device_address,
                .size = vk_buffer->size,
            },
            .reservedRangeOffset = vk_buffer->size - device->descriptor_heap_properties.minResourceHeapReservedRange,
            .reservedRangeSize = device->descriptor_heap_properties.minResourceHeapReservedRange,
        };

        vkCmdBindResourceHeapEXT(command_buffer, &bind_info);
    }

    void CommandBuffer::bind_sampler_heap(BufferID buffer) {
        VulkanBuffer *vk_buffer = device->access_buffer(buffer);
        VkBindHeapInfoEXT bind_info = {
            .sType = VK_STRUCTURE_TYPE_BIND_HEAP_INFO_EXT,
            .pNext = nullptr,
            .heapRange = {
                .address = vk_buffer->device_address,
                .size = vk_buffer->size,
            },
            .reservedRangeOffset = vk_buffer->size - device->descriptor_heap_properties.minSamplerHeapReservedRange,
            .reservedRangeSize = device->descriptor_heap_properties.minSamplerHeapReservedRange,
        };
        vkCmdBindSamplerHeapEXT(command_buffer, &bind_info);
    }
    /*
    void CommandBuffer::set_uniform_sets(PipelineID pipeline_id, const UniformSetID *uniform_sets, uint32_t uniform_set_count) {
        if (uniform_set_count == 0)
            return;
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        for (uint32_t i = 0; i < uniform_set_count; ++i) {
            VulkanUniformSet *uniform_set = device->access_uniform_set(uniform_sets[i]);
            vkCmdBindDescriptorSets(command_buffer,
                                    pipeline->bind_point, pipeline->pipeline_layout,
                                    uniform_set->set_id,
                                    1, &uniform_set->descriptor_set,
                                    0, nullptr);
        }
    }

    void CommandBuffer::set_push_constants(PipelineID pipeline_id, const PushConstant *push_constants, uint32_t push_constant_count) {
        if (push_constant_count == 0)
            return;
        VulkanPipeline *pipeline = device->access_pipeline(pipeline_id);
        for (uint32_t i = 0; i < push_constant_count; ++i) {
            const PushConstant *push_constant = &push_constants[i];
            vkCmdPushConstants(command_buffer, pipeline->pipeline_layout,
                               VkShaderStageFlags(push_constant->shader_stage),
                               push_constant->offset, push_constant->size, push_constant->data);
        }
    }
    */
    void CommandBuffer::set_push_data(uint32_t offset, const void *push_data, uint32_t push_data_size) {
        VkPushDataInfoEXT push_data_info = {
            .sType = VK_STRUCTURE_TYPE_PUSH_DATA_INFO_EXT,
            .pNext = nullptr,
            .offset = offset,
            .data = {
                .address = push_data,
                .size = push_data_size,
            },
        };
        vkCmdPushDataEXT(command_buffer, &push_data_info);
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

    void CommandBuffer::draw_indirect(BufferID indirect_buffer, uint32_t offset, uint32_t draw_count, uint32_t stride) {
        VulkanBuffer *buffer = device->access_buffer(indirect_buffer);
        vkCmdDrawIndirect(command_buffer, buffer->buffer, offset, draw_count, stride);
    }

    void CommandBuffer::dispatch(uint32_t work_size_x, uint32_t work_size_y, uint32_t work_size_z) {
        vkCmdDispatch(command_buffer, work_size_x, work_size_y, work_size_z);
    }

    void CommandBuffer::dispatch_indirect(BufferID indirect_buffer, uint32_t offset) {
        VulkanBuffer *buffer = device->access_buffer(indirect_buffer);
        vkCmdDispatchIndirect(command_buffer, buffer->buffer, offset);
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

    void CommandBuffer::copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion *regions, uint32_t copy_region_count) {
        VulkanBuffer *src_buffer = device->access_buffer(src);
        VulkanBuffer *dst_buffer = device->access_buffer(dst);

#ifdef _DEBUG
        for (uint32_t i = 0; i < copy_region_count; ++i) {
            ASSERT((regions[i].src_offset + regions[i].size) <= src_buffer->size);
            ASSERT((regions[i].dst_offset + regions[i].size) <= dst_buffer->size);
        }
#endif
        vkCmdCopyBuffer(command_buffer, src_buffer->buffer, dst_buffer->buffer, copy_region_count, (const VkBufferCopy *)regions);
    }

    void CommandBuffer::copy_texture(TextureID dst, BufferID src, uint32_t buffer_offset, uint32_t mip_count, uint32_t block_size, uint32_t bit_per_element) {
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
        pipeline_barrier(&transfer_dst_barrier, 1, nullptr, 0);

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

            buffer_offset += ((mip_width + block_size - 1) / block_size) * ((mip_height + block_size - 1) / block_size) * (bit_per_element / 8u);
            mip_width = mip_width > 1 ? mip_width / 2 : 1;
            mip_height = mip_height > 1 ? mip_height / 2 : 1;
        }
        dst_image->access_flags = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        dst_image->stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    }

    void CommandBuffer::copy_texture(TextureID dst, TextureID src, uint32_t width, uint32_t height) {
        VulkanTexture *vk_src = device->access_texture(src);
        VulkanTexture *vk_dst = device->access_texture(dst);

        VkImageCopy region = {
            .srcSubresource = {
                .aspectMask = vk_src->image_aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffset = {0, 0, 0},
            .dstSubresource = {
                .aspectMask = vk_dst->image_aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,

            },
            .dstOffset = {0, 0, 0},
            .extent = {
                width,
                height,
                1,
            },
        };

        vkCmdCopyImage(command_buffer, vk_src->image, vk_src->current_layout, vk_dst->image, vk_dst->current_layout, 1, &region);
    }

    void CommandBuffer::copy_to_swapchain(TextureID texture) {
        VulkanSwapchain *swapchain = device->get_swapchain();
        std::vector<VkImageMemoryBarrier2> image_barriers{2};
        image_barriers[0] = CreateImageMemoryBarrier2(swapchain->get_current_image(),
                                                      0,
                                                      0,
                                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                      VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                                      swapchain->get_current_image_layout(),
                                                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                                      VK_IMAGE_ASPECT_COLOR_BIT);

        VulkanTexture *src_texture = device->access_texture(texture);
        ASSERT_MSG(!is_depth_format(src_texture->format), "Copying depth to swapchain is forbidden");

        // Special case for depth texture
        image_barriers[1] = CreateImageMemoryBarrier2(src_texture->image,
                                                      src_texture->stage_mask,
                                                      src_texture->access_flags,
                                                      VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                                      VK_ACCESS_2_TRANSFER_READ_BIT,
                                                      src_texture->current_layout,
                                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                                      src_texture->image_aspect);

        src_texture->current_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        src_texture->stage_mask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        src_texture->access_flags = VK_ACCESS_2_TRANSFER_READ_BIT;
        swapchain->set_current_image_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        pipeline_barrier(image_barriers.data(), cast_u32(image_barriers.size()), nullptr, 0);

        VkImageBlit2 blit_region = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
            .pNext = nullptr,
            .srcSubresource = {
                .aspectMask = src_texture->image_aspect,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
            .srcOffsets = {{0, 0, 0}, {cast_int(src_texture->width), cast_int(src_texture->height), 1}},
            .dstSubresource = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,

            },
            .dstOffsets = {{0, 0, 0}, {cast_int(swapchain->width), cast_int(swapchain->height), 1}},
        };

        VkBlitImageInfo2 blit_image_info = {
            .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
            .pNext = nullptr,
            .srcImage = src_texture->image,
            .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            .dstImage = swapchain->get_current_image(),
            .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .regionCount = 1,
            .pRegions = &blit_region,
            .filter = VK_FILTER_LINEAR,
        };

        vkCmdBlitImage2(command_buffer, &blit_image_info);
    } // namespace mirai

    void CommandBuffer::prepare_image(const TextureBarrierInfo *barrier_infos, uint32_t barrier_count) {
        std::vector<VkImageMemoryBarrier2> image_barriers(barrier_count);

        for (uint32_t i = 0; i < barrier_count; ++i) {
            const TextureBarrierInfo *barrier_info = barrier_infos + i;
            VulkanTexture *texture = device->access_texture(barrier_info->texture_id);
            image_barriers[i] = CreateImageMemoryBarrier2(texture->image,
                                                          texture->stage_mask,
                                                          texture->access_flags,
                                                          VkPipelineStageFlags2(barrier_info->stage_mask),
                                                          VkAccessFlags(barrier_info->access_mask),
                                                          texture->current_layout,
                                                          VkImageLayout(barrier_info->layout),
                                                          texture->image_aspect);

            texture->current_layout = VkImageLayout(barrier_info->layout);
            texture->access_flags = VkAccessFlags(barrier_info->access_mask);
            texture->stage_mask = VkPipelineStageFlags2(barrier_info->stage_mask);
        }
        pipeline_barrier(image_barriers.data(), cast_u32(image_barriers.size()), nullptr, 0);
    }

    void CommandBuffer::prepare_image_for_shader_read(TextureID texture) {
        VulkanTexture *vk_image = device->access_texture(texture);
        VkImageMemoryBarrier2 shader_read_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = vk_image->stage_mask,
            .srcAccessMask = vk_image->access_flags,
            .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
            .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT,
            .oldLayout = vk_image->current_layout,
            .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = vk_image->image,
            .subresourceRange = {
                .aspectMask = vk_image->image_aspect,
                .baseMipLevel = 0,
                .levelCount = VK_REMAINING_MIP_LEVELS,
                .baseArrayLayer = 0,
                .layerCount = VK_REMAINING_ARRAY_LAYERS,
            },
        };
        pipeline_barrier(&shader_read_barrier, 1, nullptr, 0);
        vk_image->access_flags = VK_ACCESS_2_SHADER_READ_BIT;
        vk_image->stage_mask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
        vk_image->current_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    void CommandBuffer::prepare_buffer(const BufferBarrierInfo *barrier_infos, uint32_t barrier_count) {
        std::vector<VkBufferMemoryBarrier2> buffer_barriers(barrier_count);

        for (uint32_t i = 0; i < barrier_count; ++i) {
            const BufferBarrierInfo *barrier_info = barrier_infos + i;
            VulkanBuffer *buffer = device->access_buffer(barrier_info->buffer_id);
            buffer_barriers[i] = CreateBufferMemoryBarrier2(buffer->buffer,
                                                            VkPipelineStageFlags2(barrier_info->src_stage_mask),
                                                            VkAccessFlags(barrier_info->src_access_mask),
                                                            VkPipelineStageFlags2(barrier_info->dst_stage_mask),
                                                            VkAccessFlags(barrier_info->dst_access_mask),
                                                            barrier_info->offset,
                                                            barrier_info->size);
        }
        pipeline_barrier(nullptr, 0, buffer_barriers.data(), cast_u32(buffer_barriers.size()));
    }

    void CommandBuffer::set_depth_bias(float depth_bias_constant_factor, float depth_bias_clamp, float depth_bias_slope_factor) {
        vkCmdSetDepthBias(command_buffer, depth_bias_constant_factor, depth_bias_clamp, depth_bias_slope_factor);
    }

    void CommandBuffer::end_render_pass() {
        vkCmdEndRendering(command_buffer);
    }

    void CommandBuffer::begin() {
        // Reset descriptor pool
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

    void CommandBuffer::begin_gpu_debug_label(const char *name, float *colors) {
#if ENABLE_VALIDATION && ENABLE_DEBUG_LABELS
        VkDebugUtilsLabelEXT label_info = {
            .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT,
            .pLabelName = name,
        };

        if (colors == nullptr) {
            label_info.color[0] = 0.0f;
            label_info.color[1] = 1.0f;
            label_info.color[2] = 0.0f;
        } else {
            label_info.color[0] = colors[0];
            label_info.color[1] = colors[1];
            label_info.color[2] = colors[2];
        }
        label_info.color[3] = 1.0f;
        vkCmdBeginDebugUtilsLabelEXT(command_buffer, &label_info);
#endif
    }

    void CommandBuffer::end_gpu_debug_label() {
#if ENABLE_VALIDATION && ENABLE_DEBUG_LABELS
        vkCmdEndDebugUtilsLabelEXT(command_buffer);
#endif
    }
    /*
    void CommandBuffer::prepare_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node) {
        const std::vector<FrameGraphResourceState> &resources_state = node->resources_state;
        std::vector<VkImageMemoryBarrier2> image_barriers;
        for (auto &state : resources_state) {

            FrameGraphResource *resource = frame_graph->get_resource(state.resource_handle);
            if (resource->handle == K_SWAPCHAIN_TEXTURE_HANDLE) {
                prepare_swapchain_image(&state, image_barriers);
            } else {
                VulkanTexture *texture = device->access_texture(resource->handle);

                // If by some mean it has changed the layout to required layout we skip it
                if (texture->current_layout == state.layout &&
                    texture->access_flags == state.access_flags &&
                    texture->stage_mask == state.access_flags)
                    continue;

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
        pipeline_barrier(image_barriers.data(), cast_u32(image_barriers.size()), nullptr, 0);
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
    */
    void CommandBuffer::pipeline_barrier(VkImageMemoryBarrier2 *image_memory_barriers, uint32_t image_memory_barrier_count, VkBufferMemoryBarrier2 *buffer_memory_barriers, uint32_t buffer_memory_barrier_count) {
        if (image_memory_barrier_count == 0 && buffer_memory_barrier_count == 0)
            return;
        VkDependencyInfo dependency_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            .memoryBarrierCount = 0,
            .pMemoryBarriers = nullptr,
            .bufferMemoryBarrierCount = buffer_memory_barrier_count,
            .pBufferMemoryBarriers = buffer_memory_barriers,
            .imageMemoryBarrierCount = image_memory_barrier_count,
            .pImageMemoryBarriers = image_memory_barriers,
        };
        vkCmdPipelineBarrier2(command_buffer, &dependency_info);
    }

} // namespace mirai