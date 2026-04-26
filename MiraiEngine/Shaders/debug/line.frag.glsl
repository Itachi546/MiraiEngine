#version 460

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec2 in_velocity;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 velocity;

void main() {
    fragColor = vec4(in_color, 1.0f);
    velocity = in_velocity;
}
