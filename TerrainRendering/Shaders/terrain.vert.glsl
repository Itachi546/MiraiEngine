#version 450

layout(set = 0, binding = 0) buffer CBTBuffer {
    uint heap[];
};

const vec2 VERTICES[] = vec2[3](
    vec2(0.0f, 0.0f),
    vec2(0.0f, 1.0f),
    vec2(1.0f, 1.0f));

void main() {
    vec2 position = VERTICES[gl_VertexIndex];
    gl_Position = vec4(position, 0.0f, 1.0f);
}