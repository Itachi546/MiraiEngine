#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class DepthPrePass : public FrameGraphRenderer {
      public:
        DepthPrePass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DepthPrePass();

      private:
        PipelineID pipeline_id;
        UniformLayout transform_layout;
        UniformSetID per_frame_uniform_set;
    };
} // namespace mirai