#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class DeferredTransparentPass : public FrameGraphRenderer {
      public:
        DeferredTransparentPass();

        void initialize(FrameGraph *framegraph, const FrameGraphNode *node, Renderer* renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer* renderer) override;

        ~DeferredTransparentPass();

      private:
        ShaderMaterial *transparent_shader;
        UniformSetID mesh_instance_set, per_frame_uniform_set;
    };
} // namespace mirai