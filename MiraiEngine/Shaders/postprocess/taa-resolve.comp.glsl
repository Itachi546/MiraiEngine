#version 460

#extension GL_GOOGLE_include_directive : enable

#include "../utils/bindless-sampler.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform texture2D u_color;
layout(set = 0, binding = 1) uniform texture2D u_taa_history;
layout(set = 0, binding = 2) uniform texture2D u_depth;
layout(set = 0, binding = 3) uniform texture2D u_velocity;
layout(set = 0, binding = 4, rgba16f) uniform image2D u_output_texture;

layout(push_constant) uniform PushConstant {
    int width;
    int height;
    int flags;
    int _paddding;
};

#define FLAG_ENABLE_TAA 1
#define FLAG_SHOULD_SAMPLE_MOTION_VECTOR 2
#define FLAG_ENABLE_TEMPORAL_FILTERING 4
#define FLAG_TAA_SIMPLE 8
#define FLAG_ENABLE_MIN_DEPTH 16
#define FLAG_ENABLE_HISTORY_SAMPLING 32

bool has_flag(int flags, int flag) {
    return (flags & flag) == flag;
}

vec2 uv_nearest(ivec2 pixel, vec2 texture_size) {
    return (vec2(pixel) + 0.5) / texture_size;
}

vec3 taa_simple(ivec2 id) {
    vec2 image_size = vec2(width, height);
    vec2 pixel_size = 1.0f / image_size;

    vec2 uv = uv_nearest(id, image_size);
    vec3 current_sample = texture(sampler2D(u_color, u_samplers[SAMPLER_LINEAR_CLAMP]), uv).rgb;

    // Clamp color
    vec3 min_color = vec3(9999.0f);
    vec3 max_color = vec3(-9999.0f);
    float closest_depth = 1.0f;
    vec2 closest_position = vec2(0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            vec2 cuv = uv + vec2(x, y) * pixel_size;

            vec3 color = texture(sampler2D(u_color, u_samplers[SAMPLER_LINEAR_CLAMP]), cuv).rgb;
            min_color = min(min_color, color);
            max_color = max(max_color, color);

            float depth = texture(sampler2D(u_depth, u_samplers[SAMPLER_POINT_CLAMP]), cuv).r;
            if (depth < closest_depth) {
                depth = closest_depth;
                closest_position = cuv;
            }
        }
    }

    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        velocity = texture(sampler2D(u_velocity, u_samplers[SAMPLER_LINEAR_CLAMP]), closest_position).rg;
    }
    vec2 reprojected_uv = uv - velocity;
    vec3 history_sample = texture(sampler2D(u_taa_history, u_samplers[SAMPLER_LINEAR_CLAMP]), reprojected_uv).rgb;
    history_sample = clamp(history_sample, min_color, max_color);
    return current_sample * 0.1 + history_sample * 0.9;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (id.x > width - 1 || id.y > height - 1)
        return;

    if (has_flag(flags, FLAG_ENABLE_TAA)) {
        vec3 final_color = taa_simple(id);
        imageStore(u_output_texture, id, vec4(final_color, 1.0f));
    } else {
        vec2 image_size = vec2(width, height);
        vec2 uv = uv_nearest(id, image_size);
        vec3 current_sample = texture(sampler2D(u_color, u_samplers[SAMPLER_LINEAR_CLAMP]), uv).rgb;
        imageStore(u_output_texture, id, vec4(current_sample, 1.0f));
    }
}
