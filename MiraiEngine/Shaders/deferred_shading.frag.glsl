#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;
layout(set = 0, binding = 3) uniform sampler2D emissive_texture;

layout(push_constant) uniform PushConstant {
    mat4 invVP;
    vec3 camera_position;
    float _padding;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0 - 1.0, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);
    vec3 emissive = textureLod(emissive_texture, uv, 0).rgb;

    vec3 normal = octahedral_decode(normal_pbr.xy * 2.0 - 1.0);
    vec2 metallic_roughness = normal_pbr.zw;

    vec3 light_dir = normalize(vec3(-1.0f, 1.0f, 1.0f));
    float diffuse = max(dot(normal, light_dir), 0.0f);

    vec3 view_dir = normalize(camera_position - world_pos);
    vec3 halfway_vector = normalize(view_dir + light_dir);
    float ndoth = max(dot(normal, halfway_vector), 0.0);

    float specular = 0.0f; //pow(ndoth, 64.0f);

    vec3 col = (diffuse + 0.01) * albedo.rgb + specular + emissive;
    fragColor = vec4(col, 1.0f);
}