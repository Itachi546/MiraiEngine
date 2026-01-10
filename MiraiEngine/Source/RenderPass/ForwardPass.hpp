#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ForwardPass : public FrameGraphRenderer {
      public:
        ForwardPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~ForwardPass();
    };
} // namespace mirai