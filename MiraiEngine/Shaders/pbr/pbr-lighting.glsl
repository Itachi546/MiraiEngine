#ifndef PBR_LIGHTING_GLSL
#define PBR_LIGHTING_GLSL

#include "brdf.glsl"
#include "../utils/material.glsl"
#include "../utils/color.glsl"

#ifdef ENABLE_INDIRECT_LIGHTING
vec3 getIBLContribution(vec3 reflection, vec3 normal, float ndotv, vec3 F0, PBRParameter pbr_params, float ibl_contribution) {
    vec3 F = FresnelSchlickRoughness(ndotv, F0, pbr_params.roughness);
    vec3 kS = F;
    vec3 kD = 1.0f - kS;
    kD *= (1.0 - pbr_params.metallic);

    vec3 irradiance = sample_texture_cube(per_frame_data.irradiance_map, u_samplers[SAMPLER_LINEAR_CLAMP], normal).rgb;
    vec3 diffuse = irradiance * pbr_params.albedo.rgb;

    float lod = pbr_params.roughness * (per_frame_data.prefilter_mip_count - 1);
    vec3 prefilter_color = sample_texture_cube_lod(per_frame_data.prefilter_map, u_samplers[SAMPLER_LINEAR_CLAMP], reflection, lod).rgb;

    vec2 brdf = sample_texture(per_frame_data.brdf_texture_map, u_samplers[SAMPLER_LINEAR_CLAMP], vec2(ndotv, pbr_params.roughness)).rg;
    vec3 specular = prefilter_color * (F * brdf.x + brdf.y);

    return (kD * diffuse + specular) * pbr_params.ao * ibl_contribution;
}
#endif

vec3 evaluateSpecularBRDF(float roughness, vec3 F, float NdotH, float NdotL, float NdotV) {
    float alpha = roughness * roughness;
    return (D_GGX(NdotH, alpha) * F * G_Schlick_GGX(NdotL, NdotV, roughness)) / max(EPSILON, 4.0 * NdotL * NdotV);
}

vec3 evaluateDiffuseBRDF(vec3 diffuse_color) {
    return diffuse_color / PI;
}

vec3 evaluateBRDF(vec3 light_direction, vec3 view_dir, vec3 normal, PBRParameter pbr_params) {
    vec3 halfway_vector = normalize(view_dir + light_direction);
    vec3 reflection = normalize(reflect(view_dir, normal));

    float NdotL = max(dot(normal, light_direction), 0.0);
    float NdotV = max(dot(normal, view_dir), 0.0);
    float NdotH = max(dot(normal, halfway_vector), 0.0);
    float VdotH = max(dot(view_dir, halfway_vector), 0.0);

    vec3 F0 = mix(vec3(0.04), pbr_params.albedo.rgb, pbr_params.metallic);
    vec3 F = F_Schlick(F0, VdotH);
    vec3 specular = evaluateSpecularBRDF(pbr_params.roughness, F, NdotH, NdotL, NdotV);
    vec3 diffuse = evaluateDiffuseBRDF(pbr_params.albedo.rgb);

    return (1.0 - F) * diffuse + specular;
}

vec3 evaluateDirectionalLight(in Light light, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.direction;
    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;
    float ndotl = clamp(dot(normal, light_direction), 0.0f, 1.0f);
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance * ndotl;
}

// Attenuation
// GLTF recommendation: https://github.com/KhronosGroup/glTF/tree/main/extensions/2.0/Khronos/KHR_lights_punctual#range-property
// return saturate(1 - pow(dist / range, 4)) / dist2;
float getSquareFallOffAttenaution(float distance2, float radius) {
    float dist_per_range = distance2 / (radius * radius);
    dist_per_range *= dist_per_range;
    return clamp(1 - dist_per_range, 0.0, 1.0) / max(0.0001, distance2);
}

float getAngularAttenuation(vec3 light_dir, vec3 spot_dir, float inner_angle, float outer_angle) {
    float cos_outer = cos(outer_angle);
    float spot_scale = 1.0f / max(cos(inner_angle) - cos_outer, 0.001f);
    float spot_offset = -cos_outer * spot_scale;

    float angle = dot(light_dir, spot_dir);
    float attenuation = clamp(angle * spot_scale + spot_offset, 0.0, 1.0);
    return attenuation * attenuation;
}

vec3 evaluatePointLight(in Light light, in vec3 world_pos, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.position - world_pos;
    float distance2 = dot(light_direction, light_direction);
    light_direction /= sqrt(distance2);

    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;

    float ndotl = clamp(dot(normal, light_direction), 0.0f, 1.0f);
    float attenuation = getSquareFallOffAttenaution(distance2, light.radius_or_height) * ndotl;
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance * attenuation;
}

vec3 evaluateSpotLight(in Light light, in vec3 world_pos, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.position - world_pos;
    float distance2 = dot(light_direction, light_direction);
    light_direction /= sqrt(distance2);

    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;

    float ndotl = clamp(dot(normal, light_direction), 0.0f, 1.0f);
    float attenuation = getSquareFallOffAttenaution(distance2, light.radius_or_height) * ndotl;
    attenuation *= getAngularAttenuation(-light_direction, light.direction, light.inner_angle, light.outer_angle);
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance * attenuation;
}

#endif