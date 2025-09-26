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

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene* scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~DirectionalShadowPassRT();

        std::unique_ptr<ComputeShader> dir_shadow_shader;
        std::unique_ptr<ComputeShader> blur_shader;

        float sigma = 1.0f;
        float blur_sample_count = 10.0f;

      private:
        UniformSetID rt_uniform_set, blur_uniform_set_x, blur_uniform_set_y;
        TextureID blur_intermediate_texture;
        SamplerID sampler;

        void render_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
        void blur_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
    };
} // namespace mirai