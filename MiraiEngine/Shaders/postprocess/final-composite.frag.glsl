#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(set = 0, binding = 0) uniform texture2D u_input_texture;

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float enable_gamma_correction;
    float exposure;
};

#include "../utils/color.glsl"
#include "../utils/bindless-sampler.glsl"

void main() {
    vec4 col = texture(sampler2D(u_input_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), uv);

    if (enable_gamma_correction > 0.5f) {
        col.rgb *= pow(2.0, exposure);
        col.rgb = linear_to_srgb(ACESFilm(col.rgb));
    }
    fragColor = vec4(col.rgb, 1.0f);
}