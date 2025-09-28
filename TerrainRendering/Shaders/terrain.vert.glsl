#version 450

layout(set = 0, binding = 0) uniform PerFrameData {
    mat4 P;
    mat4 V;
    mat4 VP;

    vec3 camera_position;
    float elapsed_time;

    vec2 window_size;
    vec2 _padding;
};

layout(set = 1, binding = 0) readonly buffer CBTBuffer {
    uint heap[];
};

layout(set = 1, binding = 1) uniform sampler2D uHeightmap;

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "cbt.glsl"
#include "leb.glsl"
#include "terrain.glsl"

layout(location = 0) out vec3 vNormal;

void main() {
    uint nodeID = gl_InstanceIndex;
    cbtNode node = cbt_BinarySearch(nodeID);

    vec4[3] vertices = DecodeTriangleVertices(node);
    vec4 position = vertices[gl_VertexIndex];

    gl_Position = VP * position;
    vNormal = get_normal(position.xz);
}