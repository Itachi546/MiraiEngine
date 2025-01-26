#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Math/Math.hpp"

namespace mirai {

    class SSAOPass : public FrameGraphRenderer {
      public:
        SSAOPass() : FrameGraphRenderer("ssao_pass") {}

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~SSAOPass();

      private:
        std::unique_ptr<ComputeShader> shader;
        UniformSetID uniform_set;
        TextureID noise_texture;

        struct PushConstants {
            glm::mat4 inv_projection_matrix;
            float width;
            float height;
            float radius;
            float num_step;
            float step_size;
            float direction_step;
        } constant_data;
    };
} // namespace mirai