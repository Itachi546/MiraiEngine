#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;

layout(push_constant) uniform PushConstant {
    mat4 invVP;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0f - 1.0f, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);

    vec3 normal = octahedral_decode(normal_pbr.xy);
    vec2 metallic_roughness = normal_pbr.zw;

    fragColor = vec4(albedo.rgb, 1.0f);
}