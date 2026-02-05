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

#include "utils/vertexdata.glsl"

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

layout(set = 2, binding = 0) readonly buffer VertexBinding {
    Vertex vertices[];
};

layout(set = 3, binding = 0) readonly buffer TransformBinding {
    mat4 transforms[];
};

layout(set = 4, binding = 0) readonly buffer DrawDataBinding {
    DrawData draw_datas[];
};

void main() {
    DrawData draw_data = draw_datas[gl_DrawID];
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[draw_data.transform_index];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
    vec4 world_pos = M * vec4(position, 1.0f);

    vec4 current_clip_pos = per_frame_data.VP * world_pos;
    gl_Position = current_clip_pos;

    vs_out.current_clip_pos = current_clip_pos;
    vs_out.prev_clip_pos = last_frame_VP * world_pos;

    mat3 normal_matrix = mat3(transpose(inverse(M)));
    vs_out.normal = normal_matrix * u32_to_vec3(vertex.normal);
    vs_out.uv = vec2(vertex.tu, vertex.tv);
    vs_out.tangent = normal_matrix * u32_to_vec3(vertex.tangent);
    vs_out.bitangent = normal_matrix * u32_to_vec3(vertex.bitangent);
    vs_out.mat_id = draw_data.material_index;
    vs_out.world_pos = world_pos.xyz;
}