#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class DeferredPass : public FrameGraphRenderer {
      public:
        DeferredPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DeferredPass();

      private:
        UniformSetID transform_material_set;
    };
} // namespace mirai