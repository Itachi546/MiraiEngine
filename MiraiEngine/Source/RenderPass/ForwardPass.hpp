#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class ForwardPass : public FrameGraphRenderer {
      public:
        ForwardPass();

        void initialize(FrameGraph *framegraph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~ForwardPass();

      private:
        ShaderMaterial* opaque_shader;
        ShaderMaterial* transparent_shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai