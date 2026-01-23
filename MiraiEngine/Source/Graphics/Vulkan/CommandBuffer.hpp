#pragma once

#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

namespace mirai {
    class VulkanRenderingDevice;
    struct FrameGraphNode;
    class FrameGraph;
    struct FrameGraphResource;
    struct FrameGraphResourceState;

    struct TextureBarrierInfo {
        TextureID texture_id;
        uint32_t stage_mask;
        uint64_t access_mask;
        ImageLayout layout;
    };

    struct BufferBarrierInfo {
        BufferID buffer_id;
        uint64_t offset = 0;
        uint64_t size = UINT64_MAX;
        uint64_t src_stage_mask;
        uint64_t src_access_mask;
        uint64_t dst_stage_mask;
        uint64_t dst_access_mask;
    };

    class CommandBuffer {
      public:
        CommandBuffer();

        void begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph, Viewport *override_viewport = nullptr);

        void begin_compute_pass(const FrameGraphNode *node, FrameGraph *frame_graph);

        void bind_pipeline(PipelineID pipeline);

        void set_uniform_sets(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count);

        void set_push_constants(PipelineID pipeline, PushConstant *push_constants, uint32_t push_constant_count);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance);

        void draw_indexed_indirect(BufferID indirect_buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void draw_indirect(BufferID indirect_buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void dispatch(uint32_t work_size_x, uint32_t work_size_y, uint32_t work_size_z);

        void dispatch_indirect(BufferID indirect_buffer, uint32_t offset);

        void set_vertex_buffer(BufferID buffer);

        void set_index_buffer(BufferID buffer);

        void copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion *region, uint32_t region_count);

        void copy_texture(TextureID dst, BufferID src, uint32_t buffer_offset, uint32_t mip_count, uint32_t block_size);

        void copy_texture(TextureID dst, TextureID src, uint32_t dst_width, uint32_t dst_height);

        void prepare_image(const TextureBarrierInfo *barrier_infos, uint32_t barrier_count);

        void prepare_buffer(const BufferBarrierInfo *barrier_infos, uint32_t barrier_count);

        void set_depth_bias(float depth_bias_constant_factor, float depth_bias_clamp, float depth_bias_slope_factor);

        UniformSetID create_uniform_set(UniformLayout *layouts, uint32_t layout_count, uint32_t set_id);

        VkCommandBuffer get_command_buffer() {
            return command_buffer;
        }

        void end_render_pass();

        void begin();

        void wait();

      private:
        void prepare_swapchain_image(const FrameGraphResourceState *state, std::vector<VkImageMemoryBarrier2> &image_barriers);
        void prepare_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node);
        void pipeline_barrier(VkImageMemoryBarrier2 *image_memory_barriers, uint32_t image_memory_barrier_count, VkBufferMemoryBarrier2 *buffer_memory_barriers, uint32_t buffer_memory_barrier_count);

        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
        uint32_t queue_family_indices;
        VkFence fence;

        std::vector<UniformSetID> uniform_sets;

        std::vector<VkDescriptorPool> descriptor_pools;
    };
} // namespace mirai