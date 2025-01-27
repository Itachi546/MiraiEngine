#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"

layout(set = 0, binding = 0, r32f) uniform image2D u_output_blur_texture;
layout(set = 0, binding = 1) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 2) uniform sampler2D u_ssao_texture;

layout(push_constant) uniform BlurPushConstants {
    float width;
    float height;
    float direction;
    float blur_radius;

    float sharpness;
    float z_near;
    float z_far;
};

float get_linear_depth(vec2 uv) {
    float d = texture(u_depth_texture, uv).r;
    return linearize_depth(d, z_near, z_far);
}

// https://github.com/nvpro-samples/gl_ssao/blob/master/bilateralblur.frag.glsl
float blur_function(vec2 uv, float r, float center_depth, inout float w_total) {
    float sigma = blur_radius * 0.5;
    float falloff = 1.0f / (2.0 * sigma * sigma);
    float current_depth = get_linear_depth(uv);
    float current_ao = texture(u_ssao_texture, uv).r;

    float dd = (current_depth - center_depth) * sharpness;
    float w = exp2(-r * r * falloff - dd * dd);
    w_total += w;

    return current_ao * w;
}

void main() {
    ivec2 coord = ivec2(gl_GlobalInvocationID.xy);
    if (coord.x >= width || coord.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(coord) * inv_res;

    vec2 direction = direction == 0 ? vec2(1.0f, 0.0f) : vec2(0.0f, 1.0f);
    direction = direction * inv_res;

    float current_ao = texture(u_ssao_texture, uv).r;
    float current_depth = get_linear_depth(uv);

    float c_total = current_ao;
    float w_total = 1.0f;

    for (float r = 1; r <= blur_radius; ++r) {
        vec2 coord = uv + direction * r;
        c_total += blur_function(coord, r, current_depth, w_total);
    }

    for (float r = 1; r <= blur_radius; ++r) {
        vec2 coord = uv - direction * r;
        c_total += blur_function(coord, r, current_depth, w_total);
    }
    c_total /= w_total;
    imageStore(u_output_blur_texture, coord, vec4(c_total));
}
