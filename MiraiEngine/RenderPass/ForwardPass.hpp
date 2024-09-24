#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai
{
    class ForwardPass : public FrameGraphRenderPass
    {
      public:
        ForwardPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, Scene *scene) override;

        ~ForwardPass();
    };
} // namespace mirai