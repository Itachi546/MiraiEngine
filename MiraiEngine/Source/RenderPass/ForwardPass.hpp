#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class ForwardPass : public FrameGraphRenderPass {
      public:
        ForwardPass();

        void initialize(FrameGraph *framegraph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~ForwardPass();

      private:
        std::shared_ptr<ShaderMaterial> opaque_shader;
        std::shared_ptr<ShaderMaterial> transparent_shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai