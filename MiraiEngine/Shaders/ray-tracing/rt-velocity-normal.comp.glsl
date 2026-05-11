#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, r16f) uniform writeonly image2D u_velocity_texture;
layout(binding = 1, rg16f) uniform image2D u_normal_texture;

layout(push_constant) uniform PushConstantData {
    mat4 inv_VP;
    mat4 prev_VP;

    vec2 inv_resolution;
    uint depth_texture_index;
    uint _padding;
};

/*
vec3 get_view_space_normal(ivec2 iuv, vec3 P) {
    vec3 Pr = get_view_pos_from_uv(iuv + ivec2(1, 0));
    vec3 Pl = get_view_pos_from_uv(iuv + ivec2(-1, 0));
    vec3 Pt = get_view_pos_from_uv(iuv + ivec2(0, 1));
    vec3 Pb = get_view_pos_from_uv(iuv + ivec2(0, -1));

    vec3 R = min_diff(P, Pl, Pr);
    vec3 U = min_diff(P, Pb, Pt);
    return normalize(cross(U, R));
}
*/
void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    float depth = sample_texel(depth_texture_index, id, 0).r;

    vec2 uv = (id + 0.5) * inv_resolution;
    vec3 current_ndc_position = vec3(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y, depth);

    vec3 world_position = ndc_pos_to_world_pos(current_ndc_position, inv_VP);
    vec4 prev_ndc_position = prev_VP * vec4(world_position, 1.0f);
    prev_ndc_position.xyz /= prev_ndc_position.w;
}