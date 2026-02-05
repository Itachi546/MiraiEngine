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

        void set_antialiasing(bool state) {
            this->enable_aa = state;
        }

        bool is_antialiasing_enabled() const {
            return enable_aa;
        }

        ~SwapchainCopyPass();

      private:
        UniformSetID uniform_set;
        bool enable_aa;
        Shader *shader;
        SamplerID default_sampler;
    };
} // namespace mirai