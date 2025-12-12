#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ForwardPass : public FrameGraphRenderer {
      public:
        ForwardPass();

        void initialize(FrameGraph *framegraph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~ForwardPass();

      private:
        UniformLayout mesh_instance_layouts[2];
        UniformSetID per_frame_uniform_set;
    };
} // namespace mirai