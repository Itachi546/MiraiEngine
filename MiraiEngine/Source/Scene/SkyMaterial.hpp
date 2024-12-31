#pragma once

#include "ShaderMaterial.hpp"
#include "Math/Math.hpp"

namespace mirai {
    class ProceduralSkyMaterial : public ShaderMaterial {
      public:
        ProceduralSkyMaterial();

        void bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass) override;

        void set_inv_projection_matrix(const glm::mat4 &inv_p) {
            shader_inputs.inv_p = inv_p;
        }

        void set_inv_view_matrix(const glm::mat4 &inv_v) {
            shader_inputs.inv_v = inv_v;
        }

      private:
        struct ShaderInputs {
            glm::mat4 inv_p;
            glm::mat4 inv_v;
        } shader_inputs;
        PushConstant push_constant;
    };

    class SkyboxMaterial : public ShaderMaterial {
      public:
        SkyboxMaterial();

        void bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass) override;

        void set_inv_projection_matrix(const glm::mat4 &inv_p) {
            shader_inputs.inv_p = inv_p;
        }

        void set_inv_view_matrix(const glm::mat4 &inv_v) {
            shader_inputs.inv_v = inv_v;
        }

      private:
        struct ShaderInputs {
            glm::mat4 inv_p;
            glm::mat4 inv_v;
        } shader_inputs;
        PushConstant push_constant;
    };
} // namespace mirai