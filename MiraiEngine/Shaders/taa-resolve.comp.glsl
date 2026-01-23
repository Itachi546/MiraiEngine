#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_color;
layout(set = 0, binding = 1, rgba16f) uniform image2D u_taa_history;
layout(set = 0, binding = 2) uniform sampler2D u_depth;
layout(set = 0, binding = 3) uniform sampler2D u_velocity;

layout(push_constant) uniform PushConstant {
    int  width;
    int height;
    int flags;
    int _paddding;
};

#define FLAG_ENABLE_TAA 1
#define FLAG_SHOULD_SAMPLE_MOTION_VECTOR 2
#define FLAG_ENABLE_TEMPORAL_FILTERING 4
#define FLAG_TAA_SIMPLE 8

bool has_flag(int flags, int flag) {
    return (flags & flag) == flag;
}

vec2 uv_nearest(ivec2 pixel, vec2 texture_size) {
    vec2 uv = floor(pixel) + .5;
    return uv / texture_size;
}

ivec2 id_nearest(vec2 uv, vec2 texture_size) {
    return ivec2(uv * texture_size);
}

void find_closest_fragment_3x3(ivec2 pixel, out ivec2 closest_position, out float closest_depth) {
    closest_position = ivec2(0);
    closest_depth = 1.0f;

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            ivec2 pixel_position = pixel + ivec2(x, y);
            pixel_position = clamp(pixel_position, ivec2(0), ivec2(width - 1, height - 1));
            float current_depth = texelFetch(u_depth, pixel_position, 0).r;

            if (current_depth < closest_depth) {
                current_depth = closest_depth;
                closest_position = pixel_position;
            }
        }
    }
}

// https://github.com/TheRealMJP/MSAAFilter/blob/master/MSAAFilter/Resolve.hlsl
float filter_cubic(in float x, in float B, in float C) {
    float y = 0.0f;
    float x2 = x * x;
    float x3 = x * x * x;
    if (x < 1)
        y = (12 - 9 * B - 6 * C) * x3 + (-18 + 12 * B + 6 * C) * x2 + (6 - 2 * B);
    else if (x <= 2)
        y = (-B - 6 * C) * x3 + (6 * B + 30 * C) * x2 + (-12 * B - 48 * C) * x + (8 * B + 24 * C);

    return y / 6.0f;
}

float subsample_filter(float value) {
    return filter_cubic(value, 0, 0.5f);
}

vec3 taa_simple(ivec2 id) {
    vec2 image_size = vec2(width, height);
    vec2 uv = uv_nearest(id, image_size);

    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        velocity = texelFetch(u_velocity, id, 0).rg;
    }

    vec2 reprojected_uv = uv - velocity;
    vec3 taa_history_color = imageLoad(u_taa_history, id_nearest(reprojected_uv, image_size)).rgb;
    vec3 current_sample = texture(u_color, uv).rgb;

    return current_sample * 0.1 + taa_history_color * 0.9f;
}

vec3 taa(ivec2 id) {
    vec2 image_size = vec2(width, height);
    vec2 uv = uv_nearest(id, image_size);

    vec3 current_sample_total = vec3(0.0f);
    float current_sample_weight = 0.0f;
    vec3 neighbour_min = vec3(10000);
    vec3 neighbour_max = vec3(-10000);

    vec3 m1 = vec3(0);
    vec3 m2 = vec3(0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            ivec2 pixel_position = id + ivec2(x, y);
            pixel_position = clamp(pixel_position, ivec2(0), ivec2(width - 1, height - 1));
            vec3 current_sample = texelFetch(u_color, pixel_position, 0).rgb;
            vec2 subsample_position = vec2(x * 1.0, y * 1.0);
            float subsample_distance = length(subsample_position);

            float subsample_weight = subsample_filter(subsample_distance);

            current_sample_total += subsample_weight * current_sample;
            current_sample_weight += subsample_weight;

            neighbour_min = min(neighbour_min, current_sample);
            neighbour_max = max(neighbour_max, current_sample);

            m1 += current_sample;
            m2 += current_sample * current_sample;
        }
    }

    vec3 current_sample = current_sample_total / current_sample_weight;

    float closest_depth = 1.0f;
    ivec2 closest_position = ivec2(0);
    find_closest_fragment_3x3(id, closest_position, closest_depth);

    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        velocity = texelFetch(u_velocity, closest_position, 0).rg;
    }
    vec2 reprojected_uv = uv - velocity;
    if (any(lessThan(reprojected_uv, vec2(0.0f))) || any(greaterThan(reprojected_uv, vec2(1.0f)))) {
        return current_sample;
    }
    vec3 taa_history_color = imageLoad(u_taa_history, id_nearest(reprojected_uv, image_size)).rgb;
    taa_history_color = clamp(taa_history_color, neighbour_min, neighbour_max);

    vec3 current_weight = vec3(0.1f);
    vec3 history_weight = vec3(1.0f - current_weight);

    if (has_flag(flags, FLAG_ENABLE_TEMPORAL_FILTERING)) {
        vec3 temporal_weight = clamp(abs(neighbour_min - neighbour_max) / current_sample, vec3(0), vec3(1));
        history_weight = clamp(mix(vec3(0.25), vec3(0.85), temporal_weight), vec3(0), vec3(1));
        current_weight = 1.0f - history_weight;
    }

    return (current_sample * current_weight + taa_history_color * history_weight) / max(current_weight + history_weight, 0.00001);
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    vec3 final_color = vec3(0.0f);
    if (has_flag(flags, FLAG_ENABLE_TAA)) {
        if (has_flag(flags, FLAG_TAA_SIMPLE))
            final_color = taa_simple(id);
        else
            final_color = taa(id);
    } else {
        final_color = texelFetch(u_color, id, 0).rgb;
    }

    imageStore(u_taa_history, id, vec4(final_color, 1.0f));
}
