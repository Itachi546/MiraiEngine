#version 460
#extension GL_GOOGLE_include_directive : enable
#include "../utils/color.glsl"

struct Vertex {
    float sx, sy, sz;
    uint color;
};

layout(set = 0, binding = 0) readonly buffer VertexBuffer {
    Vertex vertices[];
};

layout(push_constant) uniform PushConstants {
    mat4 VP;
};

layout(location = 0) out vec3 vColor;

void main() {
    Vertex vertex = vertices[gl_VertexIndex];
    gl_Position = VP * vec4(vertex.sx, vertex.sy, vertex.sz, 1.0f);
    vColor = u32_to_rgba(vertex.color).rgb;
}