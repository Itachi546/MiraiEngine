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

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "cbt.glsl"
#include "leb.glsl"

layout(location = 0) out vec3 vColor;

mat2x3 faceVertices = mat2x3(vec3(0, 0, 1), vec3(1, 0, 0));

void main() {
    uint nodeID = gl_InstanceIndex;
    cbtNode node = cbt_LeafToHeapIndex(nodeID);
    mat3 transformMatrix = GetTransformationMatrix(node.id, int(node.depth));
    mat2x3 positionMatrix = transformMatrix * faceVertices;

    vec2 position = vec2(positionMatrix[0][gl_VertexIndex], positionMatrix[1][gl_VertexIndex]);
    position -= vec2(0.5f);
    position *= 2.0f;
    gl_Position = vec4(position.x, position.y, 0.0f, 1.0f);
    vColor = vec3(
        float(node.id % 3 == 0),
        float(node.id % 3 == 1),
        float(node.id % 3 == 2));
}