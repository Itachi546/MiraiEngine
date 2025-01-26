#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#extension GL_NV_compute_shader_derivatives : enable
#include "utils/transform.glsl"

layout(set = 0, binding = 0, r32f) uniform image2D u_ssao_texture;
layout(set = 0, binding = 1) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 2) uniform sampler2D u_noise_texture;

layout(push_constant) uniform HBAOPushConstants {
    mat4 inv_projection_matrix;
    float width;
    float height;
    float radius;
    float num_step;
    float step_size;
    float direction_step;
};

vec3 get_view_pos_from_uv(vec2 uv) {
    float depth = texture(u_depth_texture, uv).r;
    uv = uv * 2.0f - 1.0f;
    return clip_pos_to_view_pos(vec3(uv, depth), inv_projection_matrix);
}

vec3 min_diff(vec3 p, vec3 pl, vec3 pr) {
    vec3 v1 = p - pl;
    vec3 v2 = pr - p;
    return dot(v1, v1) < dot(v2, v2) ? v1 : v2;
}

#define PI 3.141592

// @TODO use shader derivatives
vec3 get_view_space_normal(vec2 uv, vec3 P, vec2 offset) {
#if 0
    vec3 Ph = dFdx(P);
    vec3 Pv = dFdy(P);
    return -normalize(cross(Ph, Pv));
#else
    vec3 Pr = get_view_pos_from_uv(uv + vec2(offset.x, 0.0f));
    vec3 Pl = get_view_pos_from_uv(uv + vec2(-offset.x, 0.0f));
    vec3 Pt = get_view_pos_from_uv(uv + vec2(0.0f, offset.y));
    vec3 Pb = get_view_pos_from_uv(uv + vec2(0.0f, -offset.y));

    vec3 R = min_diff(P, Pl, Pr);
    vec3 U = min_diff(P, Pb, Pt);

    return -normalize(cross(R, U));
#endif
}

float calculate_ao(vec2 uv, vec3 V, vec3 N) {
    const float NUM_DIRECTIONS = direction_step;
    const float NUM_STEPS = num_step;
    const float step_size = step_size;
    const float TANGENT_BIAS = 0.3f;

    vec3 rand = texture(u_noise_texture, uv * 4.).rgb;
    rand.xy *= 2.0f - 1.0f;
    mat2 rotation = mat2(rand.x, -rand.y, rand.y, rand.x);

    float d_angle = (2.0 * PI) / NUM_DIRECTIONS;
    float last_diff = 0.0f;
    float ao = 0.0f;

    for (float d = 0.0f; d < NUM_DIRECTIONS; ++d) {
        float ang = d * d_angle;
        vec2 dir = rotation * vec2(cos(ang), sin(ang));
        float tangent_angle = acos(dot(N, vec3(dir, 0.0))) - PI * 0.5 + TANGENT_BIAS;
        float horizon_angle = tangent_angle;
        vec2 p = uv + dir * step_size * rand.z;

        for (float s = 0.0f; s < NUM_STEPS; ++s) {
            vec3 S = get_view_pos_from_uv(p);
            vec3 dV = S - V;
            float length_dv = length(dV);
            if (length_dv < radius) {
                last_diff = length_dv;
                float elevation = atan(dV.z, length(dV.xy));
                horizon_angle = max(horizon_angle, elevation);
            }
            p += step_size * dir;
        }

        float norm = last_diff / radius;
        float attenuation = 1.0 - norm * norm;
        float occlusion = clamp(attenuation * (sin(horizon_angle) - sin(tangent_angle)), 0.0, 1.0);
        ao += 1.0 - occlusion;
    }
    ao /= (NUM_DIRECTIONS);
    return ao;
}

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    if (id.x >= width || id.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(id.xy) * inv_res;

    vec3 V = get_view_pos_from_uv(uv);
    vec3 N = get_view_space_normal(uv, V, inv_res);

    float ao = calculate_ao(uv, V, N);

    imageStore(u_ssao_texture, id.xy, vec4(ao, ao, ao, 1.0f));
}