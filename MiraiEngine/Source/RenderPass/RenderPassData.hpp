#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Shader.hpp"

namespace mirai {
    struct ShaderRegistry;
    struct CascadedShadowPassData {
        FrameGraphResourceHandle output;
    };

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
    };

    struct DepthPrePassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };

    struct SSAOPassData {
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle output;
        std::shared_ptr<Shader> shader;
        TextureID noise_texture;
        float noise_texture_inv_dim;
        // SSAO Shader params
        float radius;
        float intensity;
        uint32_t num_directional_step;
        uint32_t num_step;
        float tangent_bias;
    };

    struct ForwardPassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };
} // namespace mirai