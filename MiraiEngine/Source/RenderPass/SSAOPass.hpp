#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"

namespace mirai {
    struct Shader;
    class SSAOPass : public FrameGraphRenderer {
      public:
        SSAOPass() : FrameGraphRenderer("ssao_pass") {}

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        void set_blur_radius(float radius) {
            this->blur_radius = radius;
        }

        void set_blur_sharpness(float sharpness) {
            this->blur_sharpness = sharpness;
        }

        ~SSAOPass();

        float blur_radius = 10.0f;
        float blur_sharpness = 40.0f;
        float radius = 0.5f;
        struct PushConstants {
            glm::mat4 inv_projection_matrix;
            glm::vec2 ssao_texture_resolution;
            glm::vec2 depth_texture_resolution;
            glm::vec2 inv_depth_texture_resolution;
            glm::vec2 inv_noise_texture_resolution;
            float radius_to_screen;
            float neg_inv_r2;
            float num_step;
            float direction_step;
            float intensity;
            float tangent_bias;
        } constant_data;

      private:
        Shader *ssao_shader, *blur_shader;
        UniformSetID ssao_set, blur_x_set, blur_y_set;
        TextureID noise_texture, blur_intermediate_texture;

        void ssao_pass(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
        void ssao_blur(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene, float direction);
    };
} // namespace mirai