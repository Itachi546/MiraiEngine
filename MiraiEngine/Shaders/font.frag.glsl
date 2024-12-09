#version 460

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec4 fragColor;

layout(location = 0) in flat uint tex_id;
layout(location = 1) in vec2 uv;

#include "utils/bindless.glsl"

const float width = 0.3;
const float edge = 0.55;
void main() {
    float dist = 1.0 - sample_texture(tex_id, vec2(uv.x, uv.y)).r;
    float alpha = 1.0 - smoothstep(width, width + edge, dist);
    vec3 col = vec3(0.9f);
    fragColor = vec4(col, alpha);
}