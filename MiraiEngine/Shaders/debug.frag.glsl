#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    uint texture_index;
    uint n_channel;
    uint _padding[2];
};

#include "utils/bindless-sampler.glsl"
#include "utils/bindless-texture.glsl"

void main() {
    vec4 col = sample_texture(texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv);
    if (n_channel == 1) {
        col.rgb = col.rrr;
    } else if (n_channel == 2) {
        col.r = 0.0f;
    }
    fragColor = vec4(col.rgb, 1.0f);
}