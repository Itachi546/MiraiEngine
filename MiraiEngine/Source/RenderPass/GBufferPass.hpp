#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class GBufferPass : public FrameGraphRenderPass {
      public:
        GBufferPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~GBufferPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai