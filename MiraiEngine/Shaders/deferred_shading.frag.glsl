#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;

void main() {
    fragColor = textureLod(albedo_texture, uv, 0);
}