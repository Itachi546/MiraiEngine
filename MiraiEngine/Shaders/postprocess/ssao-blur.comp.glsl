#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_GOOGLE_include_directive : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/bindless-texture.glsl"

layout(set = 0, binding = 0, r16f) uniform image2D u_output_blur_texture;

#define HALF_RES 0

layout(push_constant) uniform BlurPushConstants {
    float width;
    float height;
    float blur_direction;
    float blur_radius;

    float sharpness;
    float z_near;
    float z_far;
    uint depth_texture_index;

    uint ssao_texture_index;
    uint _padding[3];
};

float guassian_weight(float x, float sigma2) {
    return exp(-x * x * 0.5 / sigma2);
}

float sample_ssao(ivec2 uv) {
    // return texture(sampler2D(u_ssao_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), uv).r;
    return sample_texel(ssao_texture_index, uv, 0).r;
}

float gaussian_blur(ivec2 coord) {
    ivec2 direction = blur_direction > 0.5 ? ivec2(0, 1) : ivec2(1, 0);
    float sigma = 3.0f;
    const float sigma2 = sigma * sigma;

    float total = sample_ssao(coord);
    float weight_sum = guassian_weight(0, sigma2);
    for (float i = 1; i < blur_radius; ++i) {
        ivec2 delta = int(i) * direction;
        float a0 = sample_ssao(coord + delta);
        float a1 = sample_ssao(coord - delta);

        float weight = guassian_weight(i, sigma2);
        total += weight * (a0 + a1);
        weight_sum += 2.0 * weight;
    }
    return total /= weight_sum;
}

float get_linear_depth(ivec2 uv) {
#if HALF_RES
    ivec2 full_res_uv = uv * 2;
    float d0 = texelFetch(u_depth_texture, full_res_uv + ivec2(1, 0), 0).r;
    float d1 = texelFetch(u_depth_texture, full_res_uv - ivec2(1, 0), 0).r;
    float d2 = texelFetch(u_depth_texture, full_res_uv + ivec2(0, 1), 0).r;
    float d3 = texelFetch(u_depth_texture, full_res_uv - ivec2(0, 1), 0).r;
    return linearize_depth((d0 + d1 + d2 + d3) * 0.25, z_near, z_far);
#else
    float depth = sample_texel(depth_texture_index, uv, 0).r;
    return linearize_depth(depth, z_near, z_far);
#endif
}

float sigma = blur_radius * 0.5;
float falloff = 1.0f / (2.0 * sigma * sigma);
float blur_function(ivec2 uv, float r, float center_depth, inout float w_total) {

    float current_depth = get_linear_depth(uv);
    float current_ao = sample_ssao(uv);

    float dd = (current_depth - center_depth) * sharpness;
    float w = exp2(-r * r * falloff - dd * dd);
    w_total += w;

    return current_ao * w;
}

float bilateral_blur(ivec2 coord) {
    float current_ao = sample_ssao(coord);
    float current_depth = get_linear_depth(coord);

    float c_total = current_ao;
    float w_total = 1.0;
    ivec2 direction = blur_direction > 0.5 ? ivec2(0, 1) : ivec2(1, 0);
    for (int r = 1; r <= blur_radius; ++r) {
        ivec2 delta = int(r) * direction;
        c_total += blur_function(coord + delta, r, current_depth, w_total);
        c_total += blur_function(coord - delta, r, current_depth, w_total);
    }

    return c_total / w_total;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(coord, vec2(width, height))))
        return;

    float result = bilateral_blur(coord);
    imageStore(u_output_blur_texture, coord, vec4(result));
}
