#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Material.hpp"

namespace mirai {
    struct ShaderRegistry;

    struct FinalCompositePassData {
        FrameGraphResourceHandle output;
        std::shared_ptr<EffectMaterial> shader;
    };

    struct DepthPrePassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };

    struct ViewNormalDepthPassData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
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
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    struct CascadedShadowPassData {
        FrameGraphResourceHandle output;
        ShaderRegistry *registry;
    };

    struct TiledLightCullPassData {
        FrameGraphResourceHandle light_list_buffer;
        uint32_t depth_texture_width;
        uint32_t depth_texture_height;
        std::shared_ptr<ComputeShader> shader;
    };

    struct ForwardPassData {
        FrameGraphResourceHandle color_texture;
        FrameGraphResourceHandle velocity_buffer;
        ShaderRegistry *registry;
        std::shared_ptr<EffectMaterial> skybox_shader;
    };

    struct RenderDebugData {
        float split_percentage;
        int debug_param_index;
        bool show_debug_cascade_color;
        bool enable_gamma_correction;
        bool light_culling;
        float exposure;
        float mip_lod_bias;

        float bloom_strength;
        float bloom_radius;
    };

    struct BloomPassData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> bloom_gen_shader;
        std::shared_ptr<ComputeShader> downsample_shader;
        std::shared_ptr<ComputeShader> upsample_shader;
    };

    struct TAAOptions {
        bool should_reset;
        bool should_sample_motion_vector;
    };

    struct TAAResolvePassData {
        FrameGraphResourceHandle output;
        TextureID history_textures[2];
        std::shared_ptr<ComputeShader> shader;
    };

} // namespace mirai