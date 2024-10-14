#version 450
#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform sampler2D u_texture;
layout(push_constant) uniform PushConstants
{
    vec2 resolution;
    float u_enable_aa;
};

#include "utils/fxaa.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

void main()
{
    if (u_enable_aa > 0.5f)
        fragColor = fxaa(u_texture, gl_FragCoord.xy, resolution);
    else
        fragColor = texture(u_texture, uv);
}