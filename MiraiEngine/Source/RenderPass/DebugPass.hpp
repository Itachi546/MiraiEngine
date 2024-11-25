#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {

    class DebugPass : public FrameGraphRenderPass {
      public:
        DebugPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;
    };
} // namespace mirai