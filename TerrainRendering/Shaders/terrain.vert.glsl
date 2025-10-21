#version 450

layout(set = 0, binding = 0) readonly buffer CBTBuffer {
    uint heap[];
};

layout(set = 0, binding = 1) uniform sampler2D uHeightmap;

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "cbt.glsl"
#include "leb.glsl"
#define FLAG_DISPLACE
#include "terrain.glsl"

layout(location = 0) out vec3 vNormal;

layout(push_constant) uniform PushConstants {
    mat4 VP;
    // width, height, maxDepth, enable_sum_reduction_prepass
    vec4 push_constant_data;
};

void main() {
    vec2 dims = push_constant_data.xy; 
    float maxHeight = push_constant_data.z;

    uint nodeID = gl_InstanceIndex;
    bool enable_sum_reduction_prepass = push_constant_data.w > 0.5 ? true : false;
    cbt_Node node = cbt_DecodeNode(nodeID, enable_sum_reduction_prepass);

    vec4[3] vertices = DecodeTriangleVertices(node, dims, maxHeight);
    vec4 position = vertices[gl_VertexIndex];

    gl_Position = VP * position;
    vNormal = get_normal(position.xz, dims);
}