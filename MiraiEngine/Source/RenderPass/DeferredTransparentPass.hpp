#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class DeferredTransparentPass : public FrameGraphRenderer {
      public:
        DeferredTransparentPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DeferredTransparentPass();
    };
} // namespace mirai