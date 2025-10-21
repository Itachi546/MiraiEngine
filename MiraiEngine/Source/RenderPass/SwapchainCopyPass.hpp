#pragma once

#include "Scene/FrameGraph.hpp"
#include <memory>

namespace mirai {
    class ShaderMaterial;
    struct FrameGraphNode;
    class FrameGraph;

    class SwapchainCopyPass : public FrameGraphRenderer {
      public:
        SwapchainCopyPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene* scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        void set_antialiasing(bool state) {
            this->enable_aa = state;
        }

        bool is_antialiasing_enabled() const {
            return enable_aa;
        }

        ~SwapchainCopyPass();

      private:
        UniformSetID uniform_set;
        ShaderMaterial* material;
        bool enable_aa;
    };
} // namespace mirai