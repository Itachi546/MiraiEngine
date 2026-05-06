#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float enable_gamma_correction;
    float exposure;

    uint color_texture_index;
    uint bloom_texture_index;
};

#include "../utils/bindless-sampler.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/color.glsl"

void main() {
#if 0
    vec4 col = sample_texture(color_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv);
    if (enable_gamma_correction > 0.5f) {
        col.rgb *= pow(2.0, exposure);
        col.rgb = linear_to_srgb(ACESFilm(col.rgb));
    }
#else
    vec4 col = sample_texture_lod(bloom_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv, 0);
#endif
    fragColor = vec4(col.rgb, 1.0f);
}