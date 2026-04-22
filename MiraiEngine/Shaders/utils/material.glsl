#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "color.glsl"
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

    float transmission;
    float texture_scale_x;
    float texture_scale_y;
    float _reserved;
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

#endif