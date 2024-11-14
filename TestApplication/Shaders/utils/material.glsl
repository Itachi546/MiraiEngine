#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

struct Material {
    vec4 albedo;
    vec3 emissive_factor;
    float metallic_factor;

    float roughness_factor;
    float transmission;
    uint shadow_flag;
    uint emissive_texture;

    uint albedo_texture;
    uint normal_texture;
    uint metallic_roughness_texture;
    uint occlusion_texture;
};

#endif