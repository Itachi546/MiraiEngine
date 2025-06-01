#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec4 albedo_buffer;
layout(location = 1) out vec4 normal_buffer;
layout(location = 2) out vec4 emissive_buffer;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec3 light_pos;
    vec3 view_dir;
    vec2 uv;
    flat uint mat_id;
}
fs_in;

#include "utils/bindless.glsl"
#include "utils/material.glsl"
#include "utils/transform.glsl"

layout(set = 3, binding = 1) readonly buffer Materials {
    Material materials[];
};

void main() {
    vec3 col = vec3(0.0f);

    Material material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE)
        albedo *= sample_texture(material.albedo_texture, fs_in.uv);

    albedo_buffer = albedo;

    vec3 n = vec3(0.0f, 0.0f, 1.0f);
    if (material.normal_texture != K_INVALID_TEXTURE)
        n = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    n = normalize(n.x * fs_in.tangent + n.y * fs_in.bitangent + n.z * fs_in.normal);
    vec2 oct_n = octahedral_encode(n) * 0.5 + 0.5;

    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (material.metallic_roughness_texture != K_INVALID_TEXTURE)
        metallic_roughness = sample_texture(material.metallic_roughness_texture, fs_in.uv).bg;

    if (is_specular_glossiness_workflow(material.flags))
        metallic_roughness.g = 1.0 - metallic_roughness.g;

    normal_buffer = vec4(oct_n, metallic_roughness);

    vec3 emissive = material.emissive_factor;
    if (material.emissive_texture != K_INVALID_TEXTURE)
        emissive *= sample_texture(material.emissive_texture, fs_in.uv).rgb;
    emissive_buffer = vec4(emissive, 1.0f);
}