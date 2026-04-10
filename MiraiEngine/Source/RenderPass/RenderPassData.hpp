#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Material.hpp"

namespace mirai {
    struct ShaderRegistry;

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
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
        ShaderRegistry *registry;
    };
} // namespace mirai