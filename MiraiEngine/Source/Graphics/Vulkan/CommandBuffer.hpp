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

    class CommandBuffer {
      public:
        CommandBuffer();

        void begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph);

        void begin_compute_pass(const FrameGraphNode *node, FrameGraph *frame_graph);

        void bind_pipeline(PipelineID pipeline, UniformSetID *uniform_sets, uint32_t uniform_set_count, PushConstant *push_constants, uint32_t push_constant_count);

        void set_uniform_sets(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count);

        void set_push_constants(PipelineID pipeline, PushConstant *push_constants, uint32_t push_constant_count);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance);

        void draw_indexed_indirect(BufferID buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void dispatch(uint32_t work_size_x, uint32_t work_size_y, uint32_t work_size_z);

        void set_vertex_buffer(BufferID buffer);

        void set_index_buffer(BufferID buffer);

        void copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion &region);

        void copy_texture(TextureID dst, BufferID src, uint32_t buffer_offset, uint32_t mip_count, uint32_t block_size);

        void prepare_image(const TextureBarrierInfo *barrier_info, uint32_t barrier_count);

        VkCommandBuffer get_command_buffer() {
            return command_buffer;
        }

        void end_render_pass();

        void begin();

        void wait();

      private:
        void prepare_swapchain_image(const FrameGraphResourceState *state, std::vector<VkImageMemoryBarrier2> &image_barriers);
        void prepare_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node);
        void pipeline_barrier(VkImageMemoryBarrier2 *image_memory_barriers, uint32_t image_memory_barrier_count);

        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
        uint32_t queue_family_indices;
        VkFence fence;
    };
} // namespace mirai