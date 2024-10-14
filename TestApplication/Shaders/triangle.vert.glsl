#version 460
#extension GL_GOOGLE_include_directive : enable

#include "utils/color.glsl"

vec2 positions[3] = vec2[](
    vec2(0.0f, -1.0f),
    vec2(-1.0f, 0.5f),
    vec2(1.0f, 1.0f));

uint colors[3] = uint[](
    0xff0000ff,
    0x00ff00ff,
    0x0000ffff);

layout(location = 0) out vec3 vColor;

void main()
{
    vec2 position = positions[gl_VertexIndex];
    gl_Position = vec4(position, 0.0f, 1.0f);

    uint color = colors[gl_VertexIndex];
    vColor = u32_to_rgba(color).rgb; 
}