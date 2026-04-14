#ifndef DIRECTIONAL_SHADOW_GLSL
#define DIRECTIONAL_SHADOW_GLSL

#include "poisson-samples.glsl"
#include "../utils/color.glsl"

#define ENABLE_SOFT_SHADOW 1
#define ENABLE_CASCADE_BLEND 1

float texture_proj(vec4 shadow_coord, vec2 offset, float bias) {
    return texture(sampler2D(u_shadow_texture, u_samplers[SAMPLER_POINT_CLAMP]), shadow_coord.xy + offset).r + bias < shadow_coord.z ? 0.0f : 1.0f;
}

float sample_shadow_disc_pcf(vec3 world_pos, int cascade_index, float pcf_radius, float pcf_sample_count) {
    if (cascade_index >= NUM_DIRLIGHT_CASCADE)
        return 1.0f;

    mat4 cascade_VP = cascade_info.VP[cascade_index];

    // Transform into light NDC Coordinate
    vec4 shadow_uv = cascade_VP * vec4(world_pos, 1.0f);
    shadow_uv /= shadow_uv.w;

    shadow_uv.xy = vec2(shadow_uv.x * 0.5 + 0.5, 0.5 - 0.5 * shadow_uv.y);
    shadow_uv.xy = clamp(shadow_uv.xy, 0.0, 1.0);

    vec2 cascade_uv = vec2(cascade_index % 2, cascade_index / 2);
    shadow_uv.xy = (cascade_uv + shadow_uv.xy) * 0.5;

    vec2 filter_size = (pcf_radius * 0.5) / vec2(cascade_info.dims[1], cascade_info.dims[2]);
    float shadow = 0.0f;
    for (int i = 0; i < int(pcf_sample_count); ++i) {
        vec2 offset = PoissonSamples[i] * filter_size;
        shadow += texture_proj(shadow_uv, offset, 0.001f);
    }
    return shadow / pcf_sample_count;
}

const float CASCADE_BLEND_REGION_PERCENT = 0.1f;
float compute_blend_factor(float cam_dist, float z_range, int cascade_index) {
    float cascade_distance = cascade_info.split_distances[cascade_index] * z_range;
    float delta = cascade_distance - cam_dist;
    float blend_region_width = CASCADE_BLEND_REGION_PERCENT * cascade_distance;

    // Is in between 1 and 0, 0 when near to the cascade end
    float blend_factor = clamp(delta / blend_region_width, 0.0, 1.0);
    return 1.0f - blend_factor;
}

float calculate_shadow_factor(vec3 world_pos, float cam_dist, out int cascade_index, float pcf_radius, float pcf_sample_count) {
    cascade_index = NUM_DIRLIGHT_CASCADE;
    float z_range = cascade_info.dims[0];
    for (int i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
        if (cam_dist <= cascade_info.split_distances[i] * z_range) {
            cascade_index = i;
            break;
        }
    }

    if (cascade_index == NUM_DIRLIGHT_CASCADE)
        return 1.0;

    float s0 = sample_shadow_disc_pcf(world_pos, cascade_index, pcf_radius, pcf_sample_count);
    float blend_factor = compute_blend_factor(cam_dist, z_range, cascade_index);
    if (cascade_index == NUM_DIRLIGHT_CASCADE - 1) {
        return mix(s0, 1.0, blend_factor);
    }

#if ENABLE_CASCADE_BLEND
    float s1 = sample_shadow_disc_pcf(world_pos, cascade_index + 1, pcf_radius, pcf_sample_count);
    return mix(s0, s1, blend_factor);
#else
    return s0;
#endif
}

vec3 get_cascade_debug_color(vec3 world_pos, float cam_dist, int cascade_index) {
    if (cascade_index == NUM_DIRLIGHT_CASCADE)
        return vec3(1.0);

    float z_range = cascade_info.dims[0];
    vec3 s0 = u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb;
    float blend_factor = compute_blend_factor(cam_dist, z_range, cascade_index);
    if (cascade_index == NUM_DIRLIGHT_CASCADE - 1) {
        return mix(s0, vec3(1.0), blend_factor);
    }

#if ENABLE_CASCADE_BLEND
    vec3 s1 = u32_to_rgba(CASCADE_COLORS[cascade_index + 1]).rgb;
    return mix(s0, s1, blend_factor);
#else
    return s0;
#endif
}

#endif