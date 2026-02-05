#version 460

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_color;
layout(set = 0, binding = 1) uniform sampler2D u_taa_history;
layout(set = 0, binding = 2) uniform sampler2D u_depth;
layout(set = 0, binding = 3) uniform sampler2D u_velocity;
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
    return (pixel + 0.5) / texture_size;
}

void find_closest_fragment_3x3(ivec2 p, out ivec2 closest_position) {
    float closest_depth = 1.0f;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            ivec2 dp = p + ivec2(x, y);
            dp = clamp(dp, ivec2(0.0f), ivec2(width - 1, height - 1));
            float depth = texelFetch(u_depth, dp, 0).r;
            if (depth < closest_depth) {
                closest_depth = depth;
                closest_position = dp;
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

float filter_catmull_rom(float value) {
    return filter_cubic(value, 0, 0.5f);
}

// Samples a texture with Catmull-Rom filtering, using 9 texture fetches instead of 16.
// See http://vec3.ca/bicubic-filtering-in-fewer-taps/ for more details
vec3 sample_texture_catmull_rom(vec2 uv, vec2 resolution) {
    // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
    // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
    // location [1, 1] in the grid, where [0, 0] is the top left corner.
    vec2 sample_position = uv * resolution;
    vec2 tex_pos_1 = floor(sample_position - 0.5f) + 0.5f;

    // Compute the fractional offset from our starting texel to our original sample location, which we'll
    // feed into the Catmull-Rom spline function to get our filter weights.
    vec2 f = sample_position - tex_pos_1;

    // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
    // These equations are pre-expanded based on our knowledge of where the texels will be located,
    // which lets us avoid having to evaluate a piece-wise function.
    vec2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
    vec2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
    vec2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
    vec2 w3 = f * f * (-0.5f + 0.5f * f);

    // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
    // simultaneously evaluate the middle 2 samples from the 4x4 grid.
    vec2 w12 = w1 + w2;
    vec2 offset_12 = w2 / (w1 + w2);

    // Compute the final UV coordinates we'll use for sampling the texture
    vec2 tex_pos_0 = tex_pos_1 - 1;
    vec2 tex_pos_3 = tex_pos_1 + 2;
    vec2 tex_pos_12 = tex_pos_1 + offset_12;

    tex_pos_0 /= resolution;
    tex_pos_3 /= resolution;
    tex_pos_12 /= resolution;

    vec3 result = vec3(0);
    result += textureLod(u_taa_history, vec2(tex_pos_0.x, tex_pos_0.y), 0).rgb * w0.x * w0.y;
    result += textureLod(u_taa_history, vec2(tex_pos_12.x, tex_pos_0.y), 0).rgb * w12.x * w0.y;
    result += textureLod(u_taa_history, vec2(tex_pos_3.x, tex_pos_0.y), 0).rgb * w3.x * w0.y;

    result += textureLod(u_taa_history, vec2(tex_pos_0.x, tex_pos_12.y), 0).rgb * w0.x * w12.y;
    result += textureLod(u_taa_history, vec2(tex_pos_12.x, tex_pos_12.y), 0).rgb * w12.x * w12.y;
    result += textureLod(u_taa_history, vec2(tex_pos_3.x, tex_pos_12.y), 0).rgb * w3.x * w12.y;

    result += textureLod(u_taa_history, vec2(tex_pos_0.x, tex_pos_3.y), 0).rgb * w0.x * w3.y;
    result += textureLod(u_taa_history, vec2(tex_pos_12.x, tex_pos_3.y), 0).rgb * w12.x * w3.y;
    result += textureLod(u_taa_history, vec2(tex_pos_3.x, tex_pos_3.y), 0).rgb * w3.x * w3.y;

    return result;
}

// Optimized clip aabb function from Inside game.
vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 previous_sample, float average_alpha) {
    // note: only clips towards aabb center (but fast!)
    vec3 p_clip = 0.5 * (aabb_max + aabb_min);
    vec3 e_clip = 0.5 * (aabb_max - aabb_min) + 0.000000001f;

    vec4 v_clip = previous_sample - vec4(p_clip, average_alpha);
    vec3 v_unit = v_clip.xyz / e_clip;
    vec3 a_unit = abs(v_unit);
    float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

    if (ma_unit > 1.0) {
        return vec4(p_clip, average_alpha) + v_clip / ma_unit;
    } else {
        // point inside aabb
        return previous_sample;
    }
}

vec3 taa_simple(ivec2 id) {
    vec2 image_size = vec2(width, height);
    vec2 uv = uv_nearest(id, image_size);

    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        velocity = texture(u_velocity, uv).rg;
    }

    vec2 reprojected_uv = uv - velocity;
    vec3 taa_history_color = texture(u_taa_history, reprojected_uv).rgb;

    vec3 current_sample = texture(u_color, uv).rgb;
    return mix(current_sample, taa_history_color, 0.9);
}

vec3 taa(ivec2 id) {
    vec2 image_size = vec2(width, height);
    vec2 uv = uv_nearest(id, image_size);

    vec2 velocity = vec2(0.0f);
    if (has_flag(flags, FLAG_SHOULD_SAMPLE_MOTION_VECTOR)) {
        ivec2 closest_position = ivec2(0);
        float closest_depth = 1.0f;

        if (has_flag(flags, FLAG_ENABLE_MIN_DEPTH))
            find_closest_fragment_3x3(id, closest_position);
        else
            closest_position = id;

        velocity = texture(u_velocity, uv_nearest(closest_position, image_size)).rg;
    }

    vec2 reprojected_uv = uv - velocity;
    vec3 history_sample = vec3(0.0f);
    if (has_flag(flags, FLAG_ENABLE_HISTORY_SAMPLING)) {
        history_sample = sample_texture_catmull_rom(reprojected_uv, image_size);
    } else {
        history_sample = texture(u_taa_history, reprojected_uv).rgb;
    }

    // Accumulate current sample and weights.
    vec3 current_sample_total = vec3(0);
    float current_sample_weight = 0.0f;
    // Min and Max used for history clipping
    vec3 neighborhood_min = vec3(10000);
    vec3 neighborhood_max = vec3(-10000);
    // Cache of moments used in the resolve phase
    vec3 m1 = vec3(0);
    vec3 m2 = vec3(0);

    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {

            ivec2 pixel_position = id + ivec2(x, y);
            pixel_position = clamp(pixel_position, ivec2(0), ivec2(width - 1, height - 1));

            vec3 current_sample = texelFetch(u_color, pixel_position, 0).rgb;
            vec2 subsample_position = vec2(x * 1.f, y * 1.f);
            float subsample_distance = length(subsample_position);
            float subsample_weight = filter_catmull_rom(subsample_distance * 2);

            current_sample_total += current_sample * subsample_weight;
            current_sample_weight += subsample_weight;

            neighborhood_min = min(neighborhood_min, current_sample);
            neighborhood_max = max(neighborhood_max, current_sample);

            m1 += current_sample;
            m2 += current_sample * current_sample;
        }
    }

    // Calculate current sample color
    vec3 current_sample = current_sample_total / current_sample_weight;

    // Guard for outside sampling
    if (any(lessThan(reprojected_uv, vec2(0.0f))) || any(greaterThan(reprojected_uv, vec2(1.0f)))) {
        return current_sample;
    }

    // Clamp history sample
    // history_sample = clamp(history_sample, neighborhood_min, neighborhood_max);
    float rcp_sample_count = 1.0f / 9.0f;
    float gamma = 1.0f;
    vec3 mu = m1 * rcp_sample_count;
    vec3 sigma = sqrt(abs((m2 * rcp_sample_count) - (mu * mu)));
    vec3 minc = mu - gamma * sigma;
    vec3 maxc = mu + gamma * sigma;

    history_sample = clip_aabb(minc, maxc, vec4(history_sample, 1), 1.0f).rgb;

    vec3 current_weight = vec3(0.1);
    vec3 history_weight = 1.0f - current_weight;

    if (has_flag(flags, FLAG_ENABLE_TEMPORAL_FILTERING)) {
        vec3 temporal_weight = clamp(abs(neighborhood_max - neighborhood_min) / current_sample, vec3(0.0f), vec3(1.0f));
        history_weight = clamp(mix(vec3(0.25), vec3(0.85), temporal_weight), vec3(0.0), vec3(1.0));
        current_weight = 1.0f - history_weight;
    }

    // vec3 current_sample = texture(u_color, uv).rgb;
    return current_sample * current_weight + history_sample * history_weight;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (has_flag(flags, FLAG_ENABLE_TAA)) {
        vec3 final_color = has_flag(flags, FLAG_TAA_SIMPLE) ? taa_simple(id) : taa(id);
        imageStore(u_output_texture, id, vec4(final_color, 1.0f));
    } else {
        vec2 image_size = vec2(width, height);
        vec2 uv = uv_nearest(id, image_size);
        vec3 current_sample = texture(u_color, uv).rgb;
        imageStore(u_output_texture, id, vec4(current_sample, 1.0f));
    }
}
