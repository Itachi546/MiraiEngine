#pragma once

#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"
namespace mirai {
    class DeferredPass : public FrameGraphRenderer {
      public:
        DeferredPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~DeferredPass();

      private:
        struct PushConstantData {
            glm::mat4 last_frame_VP;
            glm::vec2 prev_frame_jitter;
            glm::vec2 current_frame_jitter;
        } push_constant_data;
    };
} // namespace mirai