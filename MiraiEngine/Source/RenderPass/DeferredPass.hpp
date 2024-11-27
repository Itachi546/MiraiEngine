#pragma once
#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ShaderMaterial;
    class DeferredPass : public FrameGraphRenderPass {
      public:
        DeferredPass();

        void initialize(FrameGraph* frame_graph, const FrameGraphNode* node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DeferredPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        UniformSetID uniform_set;
    };
} // namespace mirai