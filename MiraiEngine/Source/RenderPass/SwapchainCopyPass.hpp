#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    struct FrameGraphNode;
    class FrameGraph;
    struct Shader;

    class SwapchainCopyPass : public FrameGraphRenderer {
      public:
        SwapchainCopyPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~SwapchainCopyPass();

        bool enable_aa;
        bool enable_gamma_correction;

      private:
        UniformSetID uniform_set;

        Shader *shader;
        SamplerID default_sampler;
    };
} // namespace mirai