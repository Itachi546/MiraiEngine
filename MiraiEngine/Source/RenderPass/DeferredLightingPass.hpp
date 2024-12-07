#pragma once
#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ShaderMaterial;
    class DeferredLightingPass : public FrameGraphRenderer {
      public:
        DeferredLightingPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DeferredLightingPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        UniformSetID uniform_set;
    };
} // namespace mirai