#version 450

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec3 vColor;

void main() {
    outColor = vec4(vColor * 0.5 + 0.5, 1.0f);
}