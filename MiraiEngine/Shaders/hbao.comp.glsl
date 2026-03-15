#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"

layout(set = 0, binding = 0, r16f) uniform image2D u_ssao_texture;
layout(set = 0, binding = 1) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 2) uniform sampler2D u_noise_texture;

layout(push_constant) uniform HBAOPushConstants {
    mat4 inv_projection_matrix;

    float width;
    float height;
    float noise_texture_width;
    float noise_texture_height;
    float depth_texture_width;
    float depth_texture_height;

    float radius_to_screen;
    float neg_inv_r2;

    float num_step;
    float direction_step;
    float intensity;
    float tangent_bias;
}
hbao;

vec3 get_view_pos_from_uv(vec2 uv) {
    float depth = textureLod(u_depth_texture, uv, 0).r;
    uv = vec2(uv.x * 2.0f - 1.0f, 1.0 - 2.0f * uv.y);
    return clip_pos_to_view_pos(vec3(uv, depth), hbao.inv_projection_matrix);
}

vec3 min_diff(vec3 p, vec3 pl, vec3 pr) {
    vec3 v1 = p - pl;
    vec3 v2 = pr - p;
    return dot(v1, v1) > dot(v2, v2) ? v1 : v2;
}

#define PI 3.141592

vec3 get_view_space_normal(vec2 uv, vec3 P, vec2 offset) {
    vec3 Pr = get_view_pos_from_uv(uv + vec2(offset.x, 0.0f));
    vec3 Pl = get_view_pos_from_uv(uv + vec2(-offset.x, 0.0f));
    vec3 Pt = get_view_pos_from_uv(uv + vec2(0.0f, offset.y));
    vec3 Pb = get_view_pos_from_uv(uv + vec2(0.0f, -offset.y));

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

float calculate_ao(vec2 uv, vec2 noise_uv, vec3 V, vec3 N) {
    const float NUM_DIRECTIONS = hbao.direction_step;
    const float NUM_STEPS = hbao.num_step;

    float radius_pixels = -hbao.radius_to_screen / V.z;
    const float step_size = radius_pixels / NUM_STEPS;

    vec3 rand = texture(u_noise_texture, noise_uv).rgb;
    float angle = rand.x * PI * 2.0;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    float d_angle = (2.0 * PI) / NUM_DIRECTIONS;
    vec2 inv_dims = 1.0f / vec2(hbao.width, hbao.height);
    float ao = 0.0f;

    for (float d = 0.0f; d < NUM_DIRECTIONS; ++d) {
        float ang = d * d_angle;
        vec2 dir = rotate_direction(vec2(cos(ang), sin(ang)), rand.yz);
        float ray_pixels = rand.y * step_size + 1.0;
        for (float s = 0.0f; s < NUM_STEPS; ++s) {
            vec2 snapped_uv = round(ray_pixels * dir) * inv_dims + uv;
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
    if (id.x > hbao.width || id.y > hbao.height)
        return;

    vec2 texel_size = 1.0f / vec2(hbao.width, hbao.height);
    vec2 uv = vec2(id.xy + 0.5) * texel_size;

    vec3 V = get_view_pos_from_uv(uv);
    vec3 N = get_view_space_normal(uv, V, texel_size);

    vec2 noise_texel_size = 1.0f / vec2(hbao.noise_texture_width, hbao.noise_texture_height);
    vec2 noise_uv = vec2(id + 0.5) * noise_texel_size;
    float ao = calculate_ao(uv, noise_uv, V, N);

    imageStore(u_ssao_texture, id.xy, vec4(ao, ao, ao, 1.0f));
}