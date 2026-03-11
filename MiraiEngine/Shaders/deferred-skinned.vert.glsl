#version 460

#extension GL_GOOGLE_include_directive : enable
#include "utils/per-frame-data.glsl"

layout(location = 0) out VS_OUT {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec3 light_pos;
    vec3 view_dir;
    vec2 uv;
    vec4 current_clip_pos;
    vec4 prev_clip_pos;
    flat uint mat_id;
}
vs_out;

layout(push_constant) uniform PushConstant {
    mat4 last_frame_VP;
    vec2 prev_frame_jitter;
    vec2 current_frame_jitter;
};

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

layout(set = 2, binding = 0) readonly buffer VertexBinding {
    uint vertices[];
};

layout(set = 3, binding = 0) readonly buffer TransformBinding {
    mat4 transforms[];
};

layout(set = 5, binding = 0) readonly buffer SkinnedMatrixPallets {
    mat4 matrix_palletes[];
};

#include "utils/vertexdata.glsl"
layout(set = 4, binding = 0) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

// Function to generate a pseudo-random color based on a uint index
vec3 hash13(uint seed) {
    // A simple, fast hash function (e.g., Wang Hash or basic LCG)
    uint x = seed * 1013904223u;
    x = (x ^ (x >> 16u)) * 0x45d9f3b;
    x = (x ^ (x >> 16u)) * 0x45d9f3b;
    x = (x ^ (x >> 16u));

    // Map the 32-bit hash to 0.0-1.0 float range for RGB
    return vec3(
        float(x & 0xFFu) / 255.0,
        float((x >> 8u) & 0xFFu) / 255.0,
        float((x >> 16u) & 0xFFu) / 255.0);
}

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    uint vertex_address = draw_data.vertex_offset + gl_VertexIndex * draw_data.vertex_stride;
    vec3 position = unpack_position(vertex_address);
    mat4 M = transforms[draw_data.transform_index];

    uvec4 joints = unpack_joints(vertex_address);
    vec4 weights = unpack_weights(vertex_address);

    mat4 skinned_matrix = matrix_palletes[joints.x] * weights.x;
    skinned_matrix += matrix_palletes[joints.y] * weights.y;
    skinned_matrix += matrix_palletes[joints.z] * weights.z;
    skinned_matrix += matrix_palletes[joints.w] * weights.w;

    mat4 world_transform = M * skinned_matrix;

    vec4 world_pos = world_transform * vec4(position, 1.0f);
    vec4 current_clip_pos = per_frame_data.VP * world_pos;
    gl_Position = current_clip_pos;

    vs_out.current_clip_pos = current_clip_pos;
    vs_out.prev_clip_pos = last_frame_VP * world_pos;

    mat3 normal_matrix = mat3(transpose(inverse(world_transform)));
    vs_out.normal = normal_matrix * unpack_normal(vertex_address);
    vs_out.tangent = normal_matrix * unpack_tangent(vertex_address);
    vs_out.bitangent = normal_matrix * unpack_bitangent(vertex_address);
    vs_out.uv = unpack_uv(vertex_address);
    vs_out.mat_id = draw_data.material_index;
    vs_out.world_pos = world_pos.xyz;
}