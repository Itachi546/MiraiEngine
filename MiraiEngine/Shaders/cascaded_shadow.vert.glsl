#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/vertexdata.glsl"
#include "utils/shadow.glsl"

layout(set = 0, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

layout(set = 1, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    uint transform_id;
    uint cascade_index;
    uint padding[2];
};

void main() {
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[transform_id];
    gl_Position = cascade_info.VP[cascade_index] * M * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
    gl_Position.z = max(gl_Position.z, -1.0f);
}