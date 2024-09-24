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

        void begin_render_pass(FrameGraphNode *node, FrameGraph *frame_graph);

        void bind_pipeline(PipelineID pipeline);

        void draw(uint32_t vertex_count, uint32_t instance_count, uint32_t first_vertex, uint32_t first_instance);

        void end_render_pass();

        void begin();

      private:
        friend class VulkanRenderingDevice;
        VulkanRenderingDevice *device;
        VkCommandBuffer command_buffer;
    };
} // namespace mirai