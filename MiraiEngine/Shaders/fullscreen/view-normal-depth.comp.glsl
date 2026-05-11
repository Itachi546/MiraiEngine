#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

// RG store view space normal and BA store view space depth
layout(binding = 0, rgba16f) uniform writeonly image2D u_normal_depth_texture;

layout(push_constant) uniform PushConstantData {
    mat4 inv_projection_matrix;
    vec2 inv_resolution;
    uint depth_texture_index;
    uint _padding;
};

vec3 get_view_pos_from_uv(ivec2 iuv) {
    vec2 uv = (iuv + 0.5f) * inv_resolution;
    // float depth = texture(sampler2D(u_depth_texture, u_samplers[SAMPLER_POINT_CLAMP]), uv).r;
    float depth = sample_texel(depth_texture_index, iuv, 0).r;
    uv = vec2(uv.x * 2.0f - 1.0f, 1.0 - 2.0f * uv.y);
    return ndc_pos_to_view_pos(vec3(uv, depth), inv_projection_matrix);
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

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec3 P = get_view_pos_from_uv(id);
    vec3 N = get_view_space_normal(id, P);

    vec4 result;
    result.xy = pack_float(P.z);
    result.zw = octahedral_encode(N);
    imageStore(u_normal_depth_texture, id, result);
}