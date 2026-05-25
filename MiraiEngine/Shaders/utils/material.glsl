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

const float C_MIN_ROUGHNESS = 0.04f;
float convert_metallic(vec3 diffuse, vec3 specular, float max_specular) {
    float perceived_diffuse = sqrt(0.299 * diffuse.r * diffuse.r + 0.587 * diffuse.g * diffuse.g + 0.114 * diffuse.b * diffuse.b);
    float perceived_specular = sqrt(0.299 * specular.r * specular.r + 0.587 * specular.g * specular.g + 0.114 * specular.b * specular.b);
    if (perceived_diffuse < C_MIN_ROUGHNESS) {
        return 0.0;
    }
    float a = C_MIN_ROUGHNESS;
    float b = perceived_diffuse * (1.0 - max_specular) / (1.0 - C_MIN_ROUGHNESS) + perceived_specular - 2.0 * C_MIN_ROUGHNESS;
    float c = C_MIN_ROUGHNESS - perceived_specular;
    float D = max(b * b - 4.0 * a * c, 0.0);
    return clamp((-b + sqrt(D)) / (2.0 * a), 0.0, 1.0);
}

vec2 fetch_pbr_metallic_roughness(PBRMaterial material, vec4 albedo, vec2 uv, float mip_bias) {
    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (is_specular_glossiness_workflow(material.flags)) {
        vec4 specular_glossiness = metallic_roughness.rrrg;
        if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
            specular_glossiness.rgb = sample_texture_lod(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0).rgb;
#else
            specular_glossiness.rgb = sample_texture_bias(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias).rgb;
#endif
        }
        metallic_roughness.y = 1.0f - specular_glossiness.a;

        const float epsilon = 1e-6;
        vec3 specular = specular_glossiness.rgb;
        float max_specular = max(specular.r, max(specular.g, specular.b));
        float metallic = convert_metallic(albedo.rgb, specular.rgb, max_specular);
        metallic_roughness.r = metallic;

        vec3 base_color_diffuse = albedo.rgb * ((1.0 - max_specular) / (1 - C_MIN_ROUGHNESS) / max(1 - metallic, epsilon));
        vec3 base_color_specular = specular - (vec3(C_MIN_ROUGHNESS) * (1 - metallic) * (1 / max(metallic, epsilon)));
        albedo = vec4(mix(base_color_diffuse, base_color_diffuse, metallic * metallic), albedo.a);
    } else {
        if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
#ifdef DISABLE_TEXTURE_DERIVATIVE
            metallic_roughness *= sample_texture_lod(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, 0).bg;
#else
            metallic_roughness *= sample_texture_bias(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], uv, mip_bias).bg;
#endif
        }
    }
    return metallic_roughness;
}

#endif
