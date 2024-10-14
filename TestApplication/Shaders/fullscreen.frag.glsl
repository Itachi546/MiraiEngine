#version 450
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform sampler2D u_texture;
#include "utils/fxaa.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

void main()
{
    fragColor = calculate_fxaa(uv); 
}