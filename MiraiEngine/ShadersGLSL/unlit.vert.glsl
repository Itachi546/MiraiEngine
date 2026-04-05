#version 460

layout(location = 0) out VS_OUT {
    vec2 uv;
    flat uint mat_id;
}
vs_out;

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    uint vertices[];
};

layout(set = 3, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

#include "utils/vertexdata.glsl"
layout(push_constant) uniform PushConstants {
    uint transform_id;
    uint material_id;
    uint padding[2];
};

void main() {
    /*
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[transform_id];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
    vec4 world_pos = M * vec4(position, 1.0f);
    gl_Position = per_frame_data.VP * world_pos;

    vs_out.uv = vec2(vertex.tu, vertex.tv);
    vs_out.mat_id = material_id;
    */
}