#pragma once

#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

namespace mirai {
    class VulkanRenderingDevice;

    struct TextureBarrierInfo {
        TextureID texture_id;
        uint32_t stage_mask;
        uint64_t access_mask;
        ImageLayout layout;
    };

    // Doesn't update underlying texture state, should be tracked externally
    struct TextureMipBarrierInfo {
        TextureID texture_id;
        uint32_t stage_mask;
        uint64_t access_mask;
        ImageLayout layout;

        uint32_t mip_level;
        uint32_t mip_count;
        uint32_t array_level;
        uint32_t array_count;
    };

    struct BufferBarrierInfo {
        BufferID buffer_id;
        uint64_t offset = 0;
        uint64_t size = UINT64_MAX;
        uint64_t dst_stage_mask;
        uint64_t dst_access_mask;
    };

    class CommandBuffer {
      public:
        CommandBuffer();

        void begin_render_pass(const std::vector<AttachmentInfo> &color_attachments, const std::optional<AttachmentInfo> &depth_attachment, uint32_t render_area_width, uint32_t render_area_height);

        // Viewport is automatically corrected to be Y-up when rendering, be careful
        void set_viewport(const Viewport &viewport);

        void set_scissor(int x, int y, uint32_t width, uint32_t height);

        void prepare_resources(const std::vector<ResourceAccessDeclaration> &resource_states);

        void bind_pipeline(PipelineID pipeline);

        void bind_resource_heap(BufferID buffer);

        void bind_sampler_heap(BufferID buffer);

        // Push data offset is the offset that should be added to base offset, rather than offset inside push data
        void set_push_data(uint32_t offset, const void *push_data, uint32_t push_data_size);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance);

        void draw_indexed_indirect(BufferID indirect_buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void draw_indirect(BufferID indirect_buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void dispatch(uint32_t work_size_x, uint32_t work_size_y, uint32_t work_size_z);

        void dispatch_indirect(BufferID indirect_buffer, uint32_t offset);

        void set_vertex_buffer(BufferID buffer);

        void set_index_buffer(BufferID buffer);

        void copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion *region, uint32_t region_count);

        void copy_texture(TextureID dst, BufferID src, uint32_t buffer_offset, uint32_t mip_count, uint32_t block_size, uint32_t bit_per_element);

        void copy_texture(TextureID dst, TextureID src, uint32_t dst_width, uint32_t dst_height);

        void copy_to_swapchain(TextureID texture);

        void prepare_image(const TextureBarrierInfo *barrier_infos, uint32_t barrier_count);
        void prepare_image_mip(const TextureMipBarrierInfo *barrier_infos, uint32_t barrier_count);

        // Uses VkImageMemoryBarrier2
        void prepare_image_for_shader_read(TextureID texture);

        void prepare_buffer(const BufferBarrierInfo *barrier_infos, uint32_t barrier_count);

        void set_depth_bias(float depth_bias_constant_factor, float depth_bias_clamp, float depth_bias_slope_factor);

        VkCommandBuffer get_command_buffer() {
            return command_buffer;
        }

        void end_render_pass();

        void begin();

        void wait();

        void begin_gpu_debug_label(const char *name, float *colors = nullptr);

        void end_gpu_debug_label();

      private:
        void pipeline_barrier(VkImageMemoryBarrier2 *image_memory_barriers, uint32_t image_memory_barrier_count, VkBufferMemoryBarrier2 *buffer_memory_barriers, uint32_t buffer_memory_barrier_count);

        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
        uint32_t queue_family_indices;
        VkFence fence;

        BufferID active_index_buffer;
        PipelineID active_pipeline;
    };
} // namespace mirai