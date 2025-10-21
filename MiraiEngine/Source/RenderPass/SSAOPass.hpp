#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Math/Math.hpp"

namespace mirai {

    class SSAOPass : public FrameGraphRenderer {
      public:
        SSAOPass() : FrameGraphRenderer("ssao_pass") {}

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene* scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        void set_blur_radius(float radius) {
            this->blur_radius = radius;
        }

        void set_blur_sharpness(float sharpness) {
            this->blur_sharpness = sharpness;
        }

        ~SSAOPass();

        float blur_radius = 2.0f;
        float blur_sharpness = 5.0f;
        struct PushConstants {
            glm::mat4 inv_projection_matrix;
            float width;
            float height;
            float radius;
            float num_step;
            float step_size;
            float direction_step;
            float intensity;
            float tangent_bias;
        } constant_data;

      private:
        std::unique_ptr<ComputeShader> ssao_shader, blur_shader;
        UniformSetID ssao_set, blur_x_set, blur_y_set;
        TextureID noise_texture, blur_intermediate_texture;

        void ssao_pass(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
        void ssao_blur(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene, float direction);
    };
} // namespace mirai