#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/color.glsl"

layout(set = 0, binding = 0) writeonly uniform image2D u_output_texture;

layout(push_constant) uniform PushConstants {
    uint input_texture_index;
    uint mip_level;
    uint width;
    uint height;
};

vec3 get_sample(vec2 uv) {
    return sample_texture_lod(input_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv, mip_level).rgb;
}

// Karis average: weight each sample by 1 / (1 + luminance)
// 0.25 because we are applying it for group of 4
float karis_weight(vec3 c) {
    return 1.0f / (1.0f + dot(c, vec3(0.2126f, 0.7152f, 0.0722f)) * 0.25f);
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    vec2 resolution = vec2(width, height);
    if (any(greaterThanEqual(id, resolution)))
        return;

    vec2 texel_size = 1.0f / resolution;
    vec2 uv = (id + 0.5) * texel_size;

    // Texel size for mip above this
    vec2 d1 = mip_level == 0 ? texel_size : texel_size * 0.5f;

    // Inner sample
    vec3 a = get_sample(uv + vec2(-d1.x, -d1.y));
    vec3 b = get_sample(uv + vec2(+d1.x, -d1.y));
    vec3 c = get_sample(uv + vec2(-d1.x, +d1.y));
    vec3 d = get_sample(uv + vec2(+d1.x, +d1.y));

    vec2 d2 = d1 * 2.0f;

    // Outer sample
    vec3 e = get_sample(uv + vec2(-d2.x, -d2.y));
    vec3 f = get_sample(uv + vec2(0, -d2.y));
    vec3 g = get_sample(uv + vec2(d2.x, -d2.y));
    vec3 h = get_sample(uv + vec2(-d2.x, 0.0f));
    vec3 i = get_sample(uv + vec2(d2.x, 0.0f));
    vec3 j = get_sample(uv + vec2(-d2.x, d2.y));
    vec3 k = get_sample(uv + vec2(0, d2.y));
    vec3 l = get_sample(uv + vec2(d2.x, d2.y));

    vec3 o = get_sample(uv);

    vec3 col = vec3(0.0f);
    if (mip_level == 0) {
        vec3 g0 = a + b + c + d;
        vec3 g1 = e + f + h + o;
        vec3 g2 = f + g + o + i;
        vec3 g3 = h + o + j + k;
        vec3 g4 = o + i + k + l;

        float w0 = karis_weight(g0);
        float w1 = karis_weight(g1);
        float w2 = karis_weight(g2);
        float w3 = karis_weight(g3);
        float w4 = karis_weight(g4);

        col = g0 * w0 * 0.5f;
        col += g1 * w1 * 0.125f;
        col += g2 * w2 * 0.125f;
        col += g3 * w3 * 0.125f;
        col += g4 * w4 * 0.125f;

        // Renormalize to preserve energy
        float total = w0 * 0.5f + (w1 + w2 + w3 + w4) * 0.125f;
        col /= total;

    } else {
        col = (a + b + c + d) * 0.125f;
        col += (e + g + j + l) * 0.03125f;
        col += (f + h + i + k) * 0.0625f;
        col += o * 0.125f;
    }
    imageStore(u_output_texture, id, vec4(col, 1.0f));
}