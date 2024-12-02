#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;

layout(push_constant) uniform PushConstant {
    mat4 invVP;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0f - 1.0f, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    fragColor = vec4(world_pos * 0.001 + albedo.rgb, 1.0f);
}