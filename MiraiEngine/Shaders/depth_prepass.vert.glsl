#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/vertexdata.glsl"

layout(set = 0, binding = 0) uniform PerFrameData {
    mat4 P;
    mat4 V;
    mat4 VP;

    vec3 camera_position;
    float elapsed_time;

    vec2 window_size;
    vec2 _padding;
};

layout(set = 1, binding = 0) readonly buffer Transform {
    mat4 transforms[];
};

layout(set = 2, binding = 0) readonly buffer VertexData {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    uint transform_id;
    uint padding[3];
};

void main() {
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[transform_id];
    gl_Position = VP * M * vec4(vertex.px, vertex.py, vertex.pz, 1.0f);
}