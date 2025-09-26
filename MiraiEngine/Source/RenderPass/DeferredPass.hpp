#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class DeferredPass : public FrameGraphRenderer {
      public:
        DeferredPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DeferredPass();

      private:
        ShaderMaterial *shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai