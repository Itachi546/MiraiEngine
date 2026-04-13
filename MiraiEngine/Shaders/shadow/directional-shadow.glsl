#ifndef DIRECTIONAL_SHADOW_GLSL
#define DIRECTIONAL_SHADOW_GLSL

#include "../utils/color.glsl"

#define ENABLE_SOFT_SHADOW 0

vec2 POISSON_DISK[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790));

float rand(vec2 uv) {
    float dot_product = dot(uv, vec2(12.9898, 78.233));
    return fract(sin(dot_product) * 43758.5453);
}

float texture_proj(vec4 shadow_coord, vec2 offset, float bias) {
    float shadow = 1.0;
    float current_depth = shadow_coord.z;
    if (current_depth > -1.0 && current_depth < 1.0) {
        float depth_from_texture = texture(sampler2D(u_shadow_texture, u_samplers[SAMPLER_POINT_CLAMP]), shadow_coord.xy + offset + bias).r;
        if (shadow_coord.w > 0.0 && depth_from_texture < current_depth)
            shadow = 0.0f;
    }
    return shadow;
}

float calculate_shadow_from_texture(vec3 world_pos, int cascade_index) {
    if (cascade_index >= NUM_DIRLIGHT_CASCADE)
        return 1.0f;

    mat4 cascade_VP = cascade_info.VP[cascade_index];

    // Transform into light NDC Coordinate
    vec4 shadow_coord = cascade_VP * vec4(world_pos, 1.0f);
    shadow_coord.xy = vec2(shadow_coord.x * 0.5 + 0.5, 0.5 - 0.5 * shadow_coord.y);

    vec2 cascade_uv = vec2(cascade_index % 2, cascade_index / 2);
    shadow_coord = shadow_coord / shadow_coord.w;
    shadow_coord.xy = (cascade_uv + clamp(shadow_coord.xy, 0.0, 1.0)) * 0.5;

    float shadow_factor = 0.0f;
    vec2 shadow_dims = vec2(cascade_info.dims[1], cascade_info.dims[2]);
    vec2 inv_res = 1.0f / shadow_dims.xy;

#if ENABLE_SOFT_SHADOW
    int k_sample_radius = 2;
    int sample_count = 0;

    const float k_pcf_radius_multiplier = 1.0f;

    vec2 delta = vec2(inv_res.x, inv_res.y) * k_pcf_radius_multiplier;
    for (int x = -k_sample_radius; x <= k_sample_radius; ++x) {
        for (int y = -k_sample_radius; y <= k_sample_radius; ++y) {
            vec2 coord = vec2(x + 0.5, y + 0.5) * 10.0f;
            int index = int(rand(world_pos.xy + coord) * 15);
            shadow_factor += texture_proj(shadow_coord, POISSON_DISK[index] * delta, 0.0f);
            sample_count++;
        }
    }
    return shadow_factor / sample_count;
#else
    return texture_proj(shadow_coord, vec2(0.0), 0.0);
#endif
}

const float CASCADE_BLEND_EDGE_LENGTH = 0.4f;
float calculate_shadow_factor(vec3 world_pos, float cam_dist, out int cascade_index) {
    cascade_index = -1;
    float z_range = cascade_info.dims[0];
    for (int i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
        if (cam_dist <= cascade_info.split_distances[i] * z_range) {
            cascade_index = i;
            break;
        }
    }

    float s0 = calculate_shadow_from_texture(world_pos, cascade_index);
    if (cascade_index == NUM_DIRLIGHT_CASCADE - 1)
        return s0;

    float start = cascade_index == 0 ? 0.0f : cascade_info.split_distances[cascade_index - 1] * z_range;
    float end = cascade_info.split_distances[cascade_index] * z_range;

    float blendFactor = (cam_dist - start) / (end - start);
    blendFactor = smoothstep(1.0f - CASCADE_BLEND_EDGE_LENGTH, 1.0f, blendFactor);
    if (blendFactor < 0.001f)
        return s0;

    float s1 = calculate_shadow_from_texture(world_pos, cascade_index + 1);
    return mix(s0, s1, blendFactor);
}

vec3 show_debug_cascade_color(vec3 world_pos, float cam_dist, out int cascade_index) {
    cascade_index = -1;
    float z_range = cascade_info.dims[0];
    for (int i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
        if (cam_dist <= cascade_info.split_distances[i] * z_range) {
            cascade_index = i;
            break;
        }
    }

    float s0 = calculate_shadow_from_texture(world_pos, cascade_index);
    if (cascade_index == NUM_DIRLIGHT_CASCADE - 1)
        return u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb;

    float start = cascade_index == 0 ? 0.0f : cascade_info.split_distances[cascade_index - 1] * z_range;
    float end = cascade_info.split_distances[cascade_index] * z_range;

    float blendFactor = (cam_dist - start) / (end - start);
    blendFactor = smoothstep(1.0f - CASCADE_BLEND_EDGE_LENGTH, 1.0f, blendFactor);
    if (blendFactor < 0.001f)
        return u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb;
    return mix(u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb,
               u32_to_rgba(CASCADE_COLORS[cascade_index + 1]).rgb, blendFactor);
}

#endif