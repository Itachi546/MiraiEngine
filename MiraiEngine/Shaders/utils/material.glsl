#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "bindless.glsl"

#define FLAG_EMPTY 0
#define FLAG_OPAQUE 1 << 0
#define FLAG_ALPHA_BLEND 1 << 1
#define FLAG_ALPHA_MASK 1 << 2
#define FLAG_DOUBLE_SIDED 1 << 3
#define FLAG_SPECULAR_GLOSSINESS_WORKFLOW 1 << 4

struct PBRMaterial {
    vec4 albedo;
    vec3 emissive_factor;
    float metallic_factor;

    float roughness_factor;
    float alpha_cutoff;
    uint flags;
    uint emissive_texture;

    uint albedo_texture;
    uint normal_texture;
    uint metallic_roughness_texture;
    uint occlusion_texture;
};

bool is_specular_glossiness_workflow(uint flags) {
    return (flags & FLAG_SPECULAR_GLOSSINESS_WORKFLOW) == FLAG_SPECULAR_GLOSSINESS_WORKFLOW;
}

struct PBRParameter {
    vec4 albedo;
    vec3 emissive;
    float metallic;
    float roughness;
    float ao;
};

PBRParameter get_pbr_material_parameter(PBRMaterial material, vec2 uv) {
    PBRParameter out_params;
    out_params.albedo = material.albedo;
    out_params.emissive = material.emissive_factor;

    if (material.albedo_texture != K_INVALID_TEXTURE)
        out_params.albedo *= sample_texture(material.albedo_texture, uv);

    if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
        vec2 mr = sample_texture(material.metallic_roughness_texture, uv).bg;
        out_params.metallic = mr.x;
        out_params.roughness = mr.y;
    } else {
        out_params.metallic = material.metallic_factor;
        out_params.roughness = material.roughness_factor;
    }

    out_params.roughness = is_specular_glossiness_workflow(material.flags) ? 1.0 - out_params.roughness : out_params.roughness;

    if (material.emissive_texture != K_INVALID_TEXTURE)
        out_params.emissive *= sample_texture(material.emissive_texture, uv).rgb;

    return out_params;
}
#endif