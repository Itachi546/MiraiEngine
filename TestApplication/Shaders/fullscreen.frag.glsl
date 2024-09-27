#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D u_texture;

void main()
{
    fragColor = vec4(texture(u_texture, uv).rgb, 1.0f);
}