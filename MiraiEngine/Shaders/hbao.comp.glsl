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

    float radius;
    float num_step;

    float _step_size;
    float direction_step;
    float intensity;
    float tangent_bias;
};

vec3 get_view_pos_from_uv(vec2 uv) {
    float depth = texture(u_depth_texture, uv).r;
    uv = vec2(uv.x * 2.0f - 1.0f, 1.0 - 2.0f * uv.y);
    return clip_pos_to_view_pos(vec3(uv, depth), inv_projection_matrix);
}

vec3 min_diff(vec3 p, vec3 pl, vec3 pr) {
    vec3 v1 = p - pl;
    vec3 v2 = pr - p;
    return dot(v1, v1) < dot(v2, v2) ? v1 : v2;
}

#define PI 3.141592

vec3 get_view_space_normal(vec2 uv, vec3 P, vec2 offset) {
    vec3 Pr = get_view_pos_from_uv(uv + vec2(offset.x, 0.0f));
    vec3 Pl = get_view_pos_from_uv(uv + vec2(-offset.x, 0.0f));
    vec3 Pt = get_view_pos_from_uv(uv + vec2(0.0f, offset.y));
    vec3 Pb = get_view_pos_from_uv(uv + vec2(0.0f, -offset.y));

    vec3 R = min_diff(P, Pl, Pr);
    vec3 U = min_diff(P, Pb, Pt);
    return -normalize(cross(R, U));
}

float calculate_ao(vec2 uv, vec2 noise_uv, vec3 V, vec3 N) {
    const float NUM_DIRECTIONS = direction_step;
    const float NUM_STEPS = num_step;
    const float step_size = radius / (-V.z * NUM_STEPS);

    vec2 rand = texture(u_noise_texture, noise_uv).rg;
    float angle = rand.x * PI * 2.0;
    mat2 rotation = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));

    float d_angle = (2.0 * PI) / NUM_DIRECTIONS;
    float ao = 0.0f;
    vec2 dims = vec2(depth_texture_width, depth_texture_height);
    vec2 inv_dims = 1.0f / dims;
    for (float d = 0.0f; d < NUM_DIRECTIONS; ++d) {
        float last_diff = 0.0f;
        float ang = d * d_angle;
        vec2 dir = normalize(rotation * vec2(cos(ang), sin(ang)));
        float tangent_angle = asin(dot(N, vec3(dir, 0.0)));

        vec2 p = round((uv + dir * step_size * rand.y) * dims) * inv_dims;
        float horizon_angle = 0.0;
        for (float s = 0.0f; s < NUM_STEPS; ++s) {
            vec3 S = get_view_pos_from_uv(p);
            vec3 dV = S - V;
            float length_dv = length(dV);
            float elevation = atan(dV.z, length(dV.xy));
            if (length_dv < radius && elevation > horizon_angle) {
                last_diff = length_dv;
                horizon_angle = elevation;
            }
            p += step_size * dir;
        }

        float norm = last_diff / radius;
        float attenuation = 1.0 - norm * norm;
        float occlusion = clamp(attenuation * (sin(horizon_angle) - sin(tangent_angle + tangent_bias)), 0.0, 1.0);
        ao += occlusion;
    }
    ao /= (NUM_DIRECTIONS);
    return 1.0f - clamp(ao * intensity, 0.0, 1.0);
}

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    if (id.x >= width || id.y >= height)
        return;

    vec2 inv_ssao_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(id.xy + 0.5) * inv_ssao_res;

    vec3 V = get_view_pos_from_uv(uv);

    vec2 depth_pixel_size = 1.0f / vec2(depth_texture_width, depth_texture_height);
    vec3 N = get_view_space_normal(uv, V, depth_pixel_size);

    vec2 noise_pixel_size = 1.0f / vec2(noise_texture_width, noise_texture_height);
    vec2 noise_uv = vec2(id + 0.5) * noise_pixel_size;
    float ao = calculate_ao(uv, noise_uv, V, N);
    imageStore(u_ssao_texture, id.xy, vec4(ao, ao, ao, 1.0f));
}