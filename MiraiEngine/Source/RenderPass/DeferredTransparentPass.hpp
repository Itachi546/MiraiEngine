#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class DeferredTransparentPass : public FrameGraphRenderer {
      public:
        DeferredTransparentPass();

        void initialize(FrameGraph *framegraph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DeferredTransparentPass();

      private:
        ShaderMaterial *transparent_shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai