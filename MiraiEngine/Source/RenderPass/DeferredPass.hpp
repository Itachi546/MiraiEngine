#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class DeferredPass : public FrameGraphRenderer {
      public:
        DeferredPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DeferredPass();
    };
} // namespace mirai