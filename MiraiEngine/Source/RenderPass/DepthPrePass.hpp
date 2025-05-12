#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ShaderMaterial;
    class DepthPrePass : public FrameGraphRenderer {
      public:
        DepthPrePass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DepthPrePass();

      private:
        ShaderMaterial* shader;
        UniformSetID transform_set;
    };
} // namespace mirai