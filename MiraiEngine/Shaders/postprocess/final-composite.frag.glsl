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
    float bloom_strength;
    float _padding;
};

#include "../utils/bindless-sampler.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/color.glsl"

void main() {
    vec4 src_color = sample_texture(color_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv);
    vec4 bloom_color = sample_texture_lod(bloom_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv, 0);
    vec3 col = mix(src_color.rgb, bloom_color.rgb, bloom_strength);
    if (enable_gamma_correction > 0.5f) {
        col *= pow(2.0, exposure);
        col = linear_to_srgb(ACESFilm(col));
    }
    fragColor = vec4(col.rgb, 1.0f);
}