#pragma once
#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ShaderMaterial;
    class DeferredLightingPass : public FrameGraphRenderer {
      public:
        DeferredLightingPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene* scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DeferredLightingPass();

      private:
        ShaderMaterial* shader;
        ShaderMaterial* rt_shader;
        UniformSetID uniform_set, cascade_uniform_set, rt_uniform_set;
    };
} // namespace mirai