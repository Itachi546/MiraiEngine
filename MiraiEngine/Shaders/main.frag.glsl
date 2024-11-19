#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec4 fragColor;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 worldPos;
    vec3 lsPos;
    vec3 viewDir;
    vec2 uv;
    flat uint matId;
}
fs_in;

#include "utils/bindless.glsl"
#include "utils/material.glsl"

layout(set = 3, binding = 1) readonly buffer Materials {
    Material materials[];
};

void main() {
    vec3 col = vec3(0.0f);

    Material material = materials[fs_in.matId];

    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE)
        albedo *= sample_texture(material.albedo_texture, fs_in.uv);
    if (albedo.a < 0.5f)
        discard;

    vec3 n = vec3(0.0f, 0.0f, 1.0f);
    if (material.normal_texture != K_INVALID_TEXTURE)
        n = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    n = normalize(n.x * fs_in.tangent + n.y * fs_in.bitangent + n.z * fs_in.normal);

    vec3 emissive = material.emissive_factor;
    if (material.emissive_texture != K_INVALID_TEXTURE)
        emissive = sample_texture(material.emissive_texture, fs_in.uv).rgb;

    col += max(dot(n, normalize(vec3(-1.0f, -1.0f, 1.0f))), 0.1f) * albedo.rgb + emissive;
    fragColor = vec4(col.rgb, 1.0f);
}