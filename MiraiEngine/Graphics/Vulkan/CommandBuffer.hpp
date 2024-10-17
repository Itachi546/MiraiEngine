#pragma once

#include "Vulkan.hpp"
#include "../RenderingDevice.hpp"

namespace mirai
{
    class VulkanRenderingDevice;
    struct FrameGraphNode;
    class FrameGraph;

    class CommandBuffer
    {
      public:
        CommandBuffer();

        void begin_render_pass(const FrameGraphNode *node, FrameGraph *frame_graph);

        void bind_pipeline(PipelineID pipeline);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void set_push_constant(PipelineID pipeline, uint32_t shader_stage, uint32_t offset, uint32_t size, void *data);

        void end_render_pass();

        void begin();

      private:
        void prepare_render_pass_resources(FrameGraph *frame_graph, const FrameGraphNode* node);
        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
    };
} // namespace mirai