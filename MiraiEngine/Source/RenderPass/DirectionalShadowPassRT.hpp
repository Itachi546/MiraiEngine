#pragma once

#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"
#include "Scene/Scene.hpp"

#include <memory>

namespace mirai {

    class ComputeShader;

    class DirectionalShadowPassRT : public FrameGraphRenderer {
      public:
        DirectionalShadowPassRT() : FrameGraphRenderer("rt_directional_shadow_pass") {
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DirectionalShadowPassRT();

        std::unique_ptr<ComputeShader> shader;

      private:
        UniformSetID rt_set;
    };
} // namespace mirai