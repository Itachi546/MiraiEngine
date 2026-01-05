#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    struct Shader;
    class DeferredLightingPass : public FrameGraphRenderer {
      public:
        DeferredLightingPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DeferredLightingPass();

      private:
        Shader *cascade_shader;
        Shader *rt_shader;
        UniformSetID cascaded_shadow_uniform_set, rt_shadow_uniform_set;
    };
} // namespace mirai