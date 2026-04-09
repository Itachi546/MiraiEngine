#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"

layout(set = 0, binding = 0, r16f) uniform image2D u_ssao_texture;
layout(set = 0, binding = 1) uniform texture2D u_depth_texture;

#define HALF_RES 0

/*
 * We do everything in integer coordinate instead of normalized uv coordinate
 * because of the issue in the normal reconstruction. While reconstructing the
 * view normal vector for uv coordinate, we get discontinuity.
 */
layout(push_constant) uniform HBAOPushConstants {
    mat4 inv_projection_matrix;

    vec2 ssao_texture_res;
    vec2 depth_texture_res;

    vec2 inv_depth_texture_res;
    vec2 inv_noise_texture_res;

    float radius_to_screen;
    float neg_inv_r2;
    float num_step;
    float direction_step;

    float intensity;
    float tangent_bias;
    uint noise_texture_index;
    uint _padding;
}
hbao;

#define PI 3.14159265359

vec2 uv_from_iuv(ivec2 p) {
    return (vec2(p) + 0.5) * hbao.inv_depth_texture_res;
}

ivec2 uv_to_iuv(vec2 uv) {
    uv = clamp(uv, 0.0, 1.0);
    return ivec2(uv * hbao.depth_texture_res);
}

vec3 get_view_pos_from_uv(ivec2 iuv) {
    vec2 uv = uv_from_iuv(iuv);
    // float depth = texture(sampler2D(u_depth_texture, u_samplers[SAMPLER_POINT_CLAMP]), uv).r;
    float depth = texelFetch(u_depth_texture, iuv, 0).r;
    uv = vec2(uv.x * 2.0f - 1.0f, 1.0 - 2.0f * uv.y);
    return clip_pos_to_view_pos(vec3(uv, depth), hbao.inv_projection_matrix);
}

vec3 min_diff(vec3 p, vec3 pl, vec3 pr) {
    vec3 v1 = p - pl;
    vec3 v2 = pr - p;
    return dot(v1, v1) < dot(v2, v2) ? v1 : v2;
}

vec3 get_view_space_normal(ivec2 iuv, vec3 P) {
    vec3 Pr = get_view_pos_from_uv(iuv + ivec2(1, 0));
    vec3 Pl = get_view_pos_from_uv(iuv + ivec2(-1, 0));
    vec3 Pt = get_view_pos_from_uv(iuv + ivec2(0, 1));
    vec3 Pb = get_view_pos_from_uv(iuv + ivec2(0, -1));

    vec3 R = min_diff(P, Pl, Pr);
    vec3 U = min_diff(P, Pb, Pt);
    return normalize(cross(U, R));
}

float falloff(float dist_sqr) {
    return dist_sqr * hbao.neg_inv_r2 + 1.0;
}

float compute_ao(vec3 p, vec3 n, vec3 s) {
    vec3 v = s - p;
    float vdotv = dot(v, v);
    float ndotv = dot(n, v) / sqrt(vdotv);
    return clamp(ndotv - hbao.tangent_bias, 0.0, 1.0) * clamp(falloff(vdotv), 0.0, 1.0);
}

vec2 rotate_direction(vec2 dir, vec2 cos_sin) {
    return vec2(dir.x * cos_sin.x - dir.y * cos_sin.y,
                dir.x * cos_sin.y + dir.y * cos_sin.x);
}

float calculate_ao(ivec2 iuv, vec2 noise_uv, vec3 V, vec3 N) {
    const float NUM_DIRECTIONS = hbao.direction_step;
    const float NUM_STEPS = hbao.num_step;

    float radius_pixels = -hbao.radius_to_screen / V.z;
    const float step_size = radius_pixels / NUM_STEPS;

    vec3 rand = sample_texture(hbao.noise_texture_index, u_samplers[SAMPLER_LINEAR_REPEAT], noise_uv * 2.0).rgb;

    float d_angle = (2.0 * PI) / NUM_DIRECTIONS;
    float ao = 0.0f;
    for (float d = 0.0f; d < NUM_DIRECTIONS; ++d) {
        float ang = d * d_angle;
        vec2 dir = rotate_direction(vec2(cos(ang), sin(ang)), rand.xy);
        float ray_pixels = rand.z * step_size + 1.0;
        for (float s = 0.0f; s < NUM_STEPS; ++s) {
            ivec2 snapped_uv = ivec2(round(ray_pixels * dir)) + iuv;
            vec3 S = get_view_pos_from_uv(snapped_uv);
            ao += compute_ao(V, N, S);
            ray_pixels += step_size;
        }
    }
    ao *= hbao.intensity / (NUM_DIRECTIONS * NUM_STEPS);
    return clamp(1.0 - ao * 2.0, 0.0, 1.0);
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (id.x > hbao.ssao_texture_res.x || id.y > hbao.ssao_texture_res.y)
        return;

#if HALF_RES
    ivec2 iuv = id.xy * 2 + 1;
#else
    ivec2 iuv = id.xy;
#endif
    vec3 V = get_view_pos_from_uv(iuv);
    vec3 N = get_view_space_normal(iuv, V);

    vec2 noise_uv = vec2(id + 0.5) * hbao.inv_noise_texture_res;
    float ao = calculate_ao(iuv, noise_uv, V, N);

    imageStore(u_ssao_texture, id.xy, vec4(vec3(ao), 1.0f));
}