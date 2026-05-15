#version 460

#extension GL_GOOGLE_include_directive : enable

#include "../utils/bindless-sampler.glsl"
#include "../utils/bindless-texture.glsl"
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0, rgba16f) uniform image2D u_output_texture;

layout(push_constant) uniform PushConstant {
    uint width;
    uint height;
    uint flags;
    uint color_texture_index;

    uint history_texture_index;
    uint depth_texture_index;
    uint velocity_texture_index;
    uint padding;
};

#define FLAG_SHOULD_ENABLE_TAA 1
#define FLAG_SHOULD_SAMPLE_MOTION_VECTOR 2

bool has_flag(uint flags, uint flag) {
    return (flags & flag) == flag;
}

vec2 uv_nearest(ivec2 pixel, vec2 texture_size) {
    return (vec2(pixel) + 0.5) / texture_size;
}

vec3 taa(vec2 uv, vec2 texel_size) {
    vec3 current_sample = sample_texture(color_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv).rgb;

    // Clamp color
    vec3 min_color = vec3(9999.0f);
    vec3 max_color = vec3(-9999.0f);
    float closest_depth = 1.0f;
    vec2 closest_position = vec2(0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 cuv = uv + vec2(x, y) * texel_size;

            vec3 color = sample_texture(color_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], cuv).rgb;
            min_color = min(min_color, color);
            max_color = max(max_color, color);

            float depth = sample_texture(depth_texture_index, u_samplers[SAMPLER_POINT_CLAMP], cuv).r;
            if (depth < closest_depth) {
                closest_depth = depth;
                closest_position = cuv;
            }
        }
    }
    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        velocity = sample_texture(velocity_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], closest_position).rg;
    }
    vec2 reprojected_uv = uv - velocity;
    vec3 history_sample = sample_texture(history_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], reprojected_uv).rgb;
    history_sample = clamp(history_sample, min_color, max_color);
    return current_sample * 0.1 + history_sample * 0.9;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    vec2 resolution = vec2(width, height);
    if (any(greaterThanEqual(id, resolution)))
        return;

    vec2 texel_size = 1.0f / resolution;
    vec2 uv = vec2(id + 0.5) * texel_size;
    if (has_flag(flags, FLAG_SHOULD_ENABLE_TAA)) {
        vec3 final_color = taa(uv, texel_size);
        imageStore(u_output_texture, id, vec4(final_color, 1.0f));
    } else {
        vec3 color = sample_texture(color_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv).rgb;
        imageStore(u_output_texture, id, vec4(color, 1.0f));
    }
}
