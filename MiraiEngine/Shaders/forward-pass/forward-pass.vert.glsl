#version 460

layout(location = 0) out VS_OUT {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec2 uv;
    flat uint mat_id;
}
vs_out;

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "../utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

layout(set = 0, binding = 1) readonly buffer VertexBinding {
    uint vertices[];
};

layout(set = 0, binding = 2) readonly buffer TransformBinding {
    mat4 transforms[];
};

#include "../utils/vertexdata.glsl"
layout(set = 0, binding = 3) readonly buffer DrawDataBindings {
    DrawData draw_datas[];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    mat4 M = transforms[draw_data.transform_index];

    uint vertex_address = draw_data.vertex_offset + gl_VertexIndex * draw_data.vertex_stride;
    vec3 position = unpack_position(vertex_address);

    vec4 world_pos = M * vec4(position, 1.0f);
    gl_Position = per_frame_data.VP * world_pos;

    mat3 normal_matrix = mat3(transpose(inverse(M)));
    vs_out.normal = normal_matrix * unpack_normal(vertex_address);
    vs_out.tangent = normal_matrix * unpack_tangent(vertex_address);
    vs_out.bitangent = normal_matrix * unpack_bitangent(vertex_address);
    vs_out.world_pos = world_pos.xyz;
    vs_out.uv = unpack_uv(vertex_address);
    vs_out.mat_id = draw_data.material_index;
}