#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/vertexdata.glsl"
#include "utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform PerFrameDataBinding {
    PerFrameData per_frame_data;
};

layout(set = 1, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    Vertex vertices[];
};

void main() {
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[gl_DrawID];
    gl_Position = per_frame_data.VP * M * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
}