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

#include "utils/vertexdata.glsl"
#include "utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    Vertex vertices[];
};

layout(set = 3, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(set = 4, binding = 0) readonly buffer DrawDataBindings {
    DrawData draw_datas[];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[draw_data.transform_index];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
    vec4 world_pos = M * vec4(position, 1.0f);
    gl_Position = per_frame_data.VP * world_pos;

    mat3 normal_matrix = mat3(transpose(inverse(M)));
    vs_out.normal = normal_matrix * u32_to_vec3(vertex.normal);
    vs_out.tangent = normal_matrix * u32_to_vec3(vertex.tangent);
    vs_out.bitangent = normal_matrix * u32_to_vec3(vertex.bitangent);
    vs_out.world_pos = world_pos.xyz;
    vs_out.uv = vec2(vertex.tu, vertex.tv);
    vs_out.mat_id = draw_data.material_index;
}