#pragma once

#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

namespace mirai {
    class VulkanRenderingDevice;
    struct FrameGraphNode;
    class FrameGraph;

    class CommandBuffer {
      public:
        CommandBuffer();

        void begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph);

        void bind_pipeline(PipelineID pipeline, UniformSetID *uniform_sets, uint32_t uniform_set_count, PushConstant *push_constants, uint32_t push_constant_count);

        void set_uniform_sets(PipelineID pipeline_id, UniformSetID *uniform_sets, uint32_t uniform_set_count);

        void set_push_constants(PipelineID pipeline, PushConstant *push_constants, uint32_t push_constant_count);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void draw_indexed(uint32_t index_count, uint32_t instance_count, uint32_t first_index, uint32_t vertex_offset, uint32_t first_instance);

        void draw_indexed_indirect(BufferID buffer, uint32_t offset, uint32_t draw_count, uint32_t stride);

        void set_vertex_buffer(BufferID buffer);

        void set_index_buffer(BufferID buffer);

        void copy_buffer(BufferID dst, BufferID src, const BufferCopyRegion &region);

        void end_render_pass();

        void begin();

        void wait();

      private:
        void prepare_render_pass_resources(FrameGraph *frame_graph, const FrameGraphNode *node);
        void prepare_input_resources(FrameGraph *frame_graph, const FrameGraphNode *node);
        void prepare_output_resources(FrameGraph *frame_graph, const FrameGraphNode *node);

        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
        uint32_t queue_family_indices;
        VkFence fence;
    };
} // namespace mirai