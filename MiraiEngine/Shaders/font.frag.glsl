#version 460

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec4 fragColor;

layout(location = 0) in flat uint tex_id;
layout(location = 1) in vec2 uv;

#include "utils/bindless.glsl"

void main() {
    float col = sample_texture(tex_id, vec2(uv.x, uv.y)).r;
    fragColor = vec4(col, col, col, 1.0f);
}