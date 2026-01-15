#version 460

#extension GL_GOOGLE_include_directive : enable

#include "utils/bindless.glsl"
#include "utils/material.glsl"

layout(location = 0) in FS_IN {
    vec2 uv;
    flat uint mat_id;
}
fs_in;

layout(set = 3, binding = 1) readonly buffer Materials {
    PBRMaterial materials[];
};

void main() {
    PBRMaterial material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE)
        albedo *= sample_texture(material.albedo_texture, fs_in.uv);

    if (albedo.a <= material.alpha_cutoff)
        discard;
}