#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class ForwardPass : public FrameGraphRenderPass {
      public:
        ForwardPass(const std::string &name);

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        ~ForwardPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai