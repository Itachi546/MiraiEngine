#ifndef FXAA_GLSL
#define FXAA_GLSL

#include "color.glsl"

const float EDGE_THRESHOLD_MIN = 0.0312;
const float EDGE_THRESHOLD_MAX = 0.125;

layout(push_constant) uniform PushConstants
{
    vec2 u_inv_screen_size;
};

vec4 calculate_fxaa(vec2 uv)
{
    float luma_up = rgb_to_luma(textureOffset(u_texture, uv, ivec2(0.0f, -1.0f)).rgb);
    float luma_down = rgb_to_luma(textureOffset(u_texture, uv, ivec2(0.0f, 1.0f)).rgb);

    float luma_left = rgb_to_luma(textureOffset(u_texture, uv, ivec2(-1.0f, 0.0f)).rgb);
    float luma_right = rgb_to_luma(textureOffset(u_texture, uv, ivec2(1.0f, 0.0f)).rgb);

    vec4 center_color = texture(u_texture, uv);
    float luma_center = rgb_to_luma(center_color.rgb);

    float luma_min = min(luma_up, min(luma_down, min(luma_left, min(luma_right, luma_center))));
    float luma_max = max(luma_up, max(luma_down, max(luma_left, max(luma_right, luma_center))));

    // if the contrast different is small or too low we ignore it
    float luma_range = luma_max - luma_min;
    if (luma_range < max(EDGE_THRESHOLD_MIN, luma_max * EDGE_THRESHOLD_MAX))
        return center_color;

    float luma_up_left = rgb_to_luma(textureOffset(u_texture, uv, ivec2(-1.0f, -1.0f)).rgb);
    float luma_down_left = rgb_to_luma(textureOffset(u_texture, uv, ivec2(-1.0f, 1.0f)).rgb);

    float luma_up_right = rgb_to_luma(textureOffset(u_texture, uv, ivec2(1.0f, -1.0f)).rgb);
    float luma_down_right = rgb_to_luma(textureOffset(u_texture, uv, ivec2(1.0f, 1.0f)).rgb);

    // Find the horizontal edges and vertical edges
    // using following filter
    // | 1 | 2 | 1 |
    // | 2 |   | 2 |
    // | 1 | 2 | 1 |
    float luma_left_corners = luma_down_left + luma_up_left;
    float luma_right_corners = luma_down_right + luma_up_right;

    float luma_up_corners = luma_up_left + luma_up_right;
    float luma_down_corners = luma_down_left + luma_down_right;

    float luma_down_up = luma_down + luma_up;
    float luma_left_right = luma_left + luma_right;

    float edge_horizontal = abs(-2.0 * luma_left + luma_left_corners) +
                            abs(-2.0 * luma_center + luma_down_up) * 2.0 +
                            abs(-2.0 * luma_right + luma_right_corners);
    float edge_vertical = abs(-2.0 * luma_up + luma_up_corners) +
                          abs(-2.0 * luma_center + luma_left_right) * 2.0 +
                          abs(-2.0 * luma_down + luma_down_corners);

    bool is_horizontal = (edge_horizontal >= edge_vertical);

    float luma1 = is_horizontal ? luma_down : luma_left;
    float luma2 = is_horizontal ? luma_up : luma_right;

    float gradient1 = luma1 - luma_center;
    float gradient2 = luma2 - luma_center;

    // Selecte steepest direction
    bool is_1steepest = abs(gradient1) >= abs(gradient2);
    float gradient_scaled = 0.25 * max(abs(gradient1), abs(gradient2));

    // Search for end of edge
    return vec4(u_inv_screen_size, 1.0f, 1.0f);
}

#endif