#version 460

vec2 positions[3] = vec2[](
    vec2(-1.0f, -1.0f),
    vec2(3.0f, -1.0f),
    vec2(-1.0f, 3.0f));

layout(location = 0) out vec2 uv;

void main() {
    vec2 position = positions[gl_VertexIndex];
    gl_Position = vec4(position, 1.0f, 1.0f);
    uv = vec2(position.x * 0.5 + 0.5, 0.5 - position.y * 0.5);
}