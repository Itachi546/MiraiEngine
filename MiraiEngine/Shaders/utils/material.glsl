#ifndef MATERIAL_GLSL
#define MATERIAL_GLSL

#include "color.glsl"
#define FLAG_METALLIC_ROUGHNESS_WORKFLOW 1
#define FLAG_SPECULAR_GLOSSINESS_WORKFLOW 2
#define FLAG_NO_GLOSSINESS_CHANNEL 4

struct PBRMaterial {
    vec4 albedo;

    vec3 specular_factor;
    float glossiness;

    float metallic_factor;
    float roughness_factor;
    float transmission;
    float thickness;

    vec3 emissive_factor;
    uint flags;

    float alpha_cutoff;
    uint emissive_texture;
    uint albedo_texture;
    uint normal_texture;

    uint pbr_texture;
    uint occlusion_texture;
    float texture_scale_x;
    float texture_scale_y;
};

bool is_specular_glossiness_workflow(uint flags) {
    return (flags & FLAG_SPECULAR_GLOSSINESS_WORKFLOW) == FLAG_SPECULAR_GLOSSINESS_WORKFLOW;
}

bool has_glossiness_channel(uint flags) {
    return (flags & FLAG_NO_GLOSSINESS_CHANNEL) == FLAG_NO_GLOSSINESS_CHANNEL;
}

struct PBRParameter {
    vec4 albedo;
    vec3 emissive;
    float metallic;
    float roughness;
    float ao;
};

bool is_valid(uint texture) {
    return texture != K_INVALID_TEXTURE;
}

vec4 fetch_albedo(PBRMaterial material, vec2 uv, float mip_bias) {
    vec4 albedo = material.albedo;
    if (is_valid(material.albedo_texture)) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
        albedo *= sample_texture_lod(material.albedo_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0);
#else
        albedo *= sample_texture_bias(material.albedo_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias);
#endif
    }
    return albedo;
}

vec3 fetch_normal_map(PBRMaterial material, vec2 uv, float mip_bias) {
    vec3 normal = vec3(0.0f, 0.0f, 1.0f);
    if (is_valid(material.normal_texture)) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
        normal = sample_texture_lod(material.normal_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0).rgb * 2.0f - 1.0f;
#else
        normal = sample_texture_bias(material.normal_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias).rgb * 2.0f - 1.0f;
#endif
    }
    return normal;
}

vec3 fetch_emissive(PBRMaterial material, vec2 uv, float mip_bias) {
    vec3 emissive = material.emissive_factor;
    if (is_valid(material.emissive_texture)) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
        emissive *= sample_texture_lod(material.emissive_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0).rgb;
#else
        emissive *= sample_texture_bias(material.emissive_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias).rgb;
#endif
    }
    return emissive;
}
// https://kcoley.github.io/glTF/extensions/2.0/Khronos/KHR_materials_pbrSpecularGlossiness/examples/convert-between-workflows-bjs/
const float epsilon = 10e-6;
vec3 dielectric_specular = vec3(0.04);
float solve_metallic(float diffuse, float specular, float one_minus_specular_strength) {
    if (specular < dielectric_specular.r)
        return 0.0f;

    float a = dielectric_specular.r;
    float b = diffuse * one_minus_specular_strength / (1 - dielectric_specular.r) + specular - 2.0 * dielectric_specular.r;
    float c = dielectric_specular.r - specular;
    float D = b * b - 4 * a * c;
    return clamp((-b + sqrt(D)) / (2 * a), 0.0f, 1.0f);
}

float get_perceived_brightness(vec3 color) {
    vec3 squared = color * color;
    return sqrt(dot(squared, vec3(0.299, 0.587, 0.114)));
}

vec2 fetch_pbr_metallic_roughness(PBRMaterial material, vec2 uv, float mip_bias) {
    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (is_specular_glossiness_workflow(material.flags)) {
        vec3 specular = material.specular_factor;
        float glossiness = material.glossiness;
        if (material.pbr_texture != K_INVALID_TEXTURE) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
            vec4 specular_glossiness_factor = sample_texture_lod(material.pbr_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0);
#else
            vec4 specular_glossiness_factor = sample_texture_bias(material.pbr_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias);
#endif
            specular = specular_glossiness_factor.rgb;
            if (has_glossiness_channel(material.flags))
                glossiness = specular_glossiness_factor.a;
        }
        // Convert to metallic roughness workflow
        float one_minus_specular_strength = 1.0f - max(specular.r, max(specular.g, specular.b));
        float metallic = solve_metallic(get_perceived_brightness(material.albedo.rgb), get_perceived_brightness(specular), one_minus_specular_strength);

        vec3 base_color_from_diffuse = material.albedo.rgb * (one_minus_specular_strength / (1.0 - dielectric_specular.r)) / max(1.0 - metallic, epsilon);
        vec3 base_color_from_specular = (specular - dielectric_specular * (1.0 - metallic)) / max(metallic, epsilon);
        vec3 base_color = clamp(mix(base_color_from_diffuse, base_color_from_specular, metallic * metallic), 0.0, 1.0);
        material.albedo.rgb = base_color;
        metallic_roughness.r = metallic;
        metallic_roughness.g = 1 - glossiness;
    } else {
        if (material.pbr_texture != K_INVALID_TEXTURE) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
            metallic_roughness *= sample_texture_lod(material.pbr_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0).bg;
#else
            metallic_roughness *= sample_texture_bias(material.pbr_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias).bg;
#endif
        }
    }
    return metallic_roughness;
}

#endif
