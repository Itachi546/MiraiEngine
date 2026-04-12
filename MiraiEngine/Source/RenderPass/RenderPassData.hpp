#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Material.hpp"

namespace mirai {
    struct ShaderRegistry;

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
        FrameGraphResourceHandle input;
        std::shared_ptr<ComputeShader> shader;
    };

    struct DepthPrePassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };

    struct HBAOParams {
        TextureID noise_texture;
        float noise_texture_inv_dim;
        // SSAO Shader params
        float radius;
        float intensity;
        int num_directional_step;
        int num_step;
        float tangent_bias;

        float blur_sharpness;
        float blur_radius;
    };

    struct SSAOPassData {
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    struct CascadedShadowPassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };

    struct ForwardPassData {
        FrameGraphResourceHandle output;
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle ssao_texture;
        FrameGraphResourceHandle csm_texture;
        ShaderRegistry *registry;
        std::shared_ptr<ShaderMaterial> skybox_shader;
    };

    struct RenderDebugData {
        float split_percentage;
        int debug_param_index;
        bool show_debug_cascade_color;
        bool enable_gamma_correction;
    };

    struct DeferredOverlay3DPassData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ShaderMaterial> skybox_shader;
    };
} // namespace mirai