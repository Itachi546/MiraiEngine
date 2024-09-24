#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai
{
    class FullScreenPass : public FrameGraphRenderPass
    {
      public:
        FullScreenPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, Scene *scene) override;

        ~FullScreenPass();
    };
} // namespace mirai