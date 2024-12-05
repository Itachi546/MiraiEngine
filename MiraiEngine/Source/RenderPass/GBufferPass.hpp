#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai {
    class ShaderMaterial;
    class GBufferPass : public FrameGraphRenderer {
      public:
        GBufferPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~GBufferPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        UniformSetID mesh_instance_set;
    };
} // namespace mirai