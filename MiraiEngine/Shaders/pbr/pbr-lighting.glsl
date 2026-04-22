#ifndef PBR_LIGHTING_GLSL
#define PBR_LIGHTING_GLSL

#include "pbr.glsl"
#include "../utils/material.glsl"
#include "../utils/color.glsl"

vec3 getIBLContribution(vec3 reflection, vec3 normal, float ndotv, vec3 F0, PBRParameter pbr_params, float ibl_contribution) {
    /*
    vec3 Ks = F_SchlickRoughness(ndotv, F0, pbr_params.roughness);
    vec3 Kd = (1.0 - Ks) * (1.0 - pbr_params.metallic);
    vec3 irradiance = sample_texture_cube(per_frame_data.irradiance_map, normal).rgb;
    vec3 diffuse = irradiance * pbr_params.albedo.rgb / PI;
    vec3 R = normalize(reflect(-view_dir, normal));
    vec3 prefilter_color = sample_texture_cube_lod(per_frame_data.prefilter_map, R, pbr_params.roughness * (MAX_REFLECTION_LOD - 1)).rgb;
    vec2 brdf = sample_texture(per_frame_data.brdf_texture_map, vec2(ndotv, pbr_params.roughness)).rg;
    vec3 specular = prefilter_color * (Ks * brdf.x + brdf.y);

    vec3 ambient = (Kd * diffuse + specular) * pbr_params.ao;
    return ambient;
    */
    vec2 brdf = sample_texture(per_frame_data.brdf_texture_map, u_samplers[SAMPLER_LINEAR_CLAMP], vec2(ndotv, pbr_params.roughness)).rg;
    vec3 diffuse_light = sample_texture_cube(per_frame_data.irradiance_map, u_samplers[SAMPLER_LINEAR_CLAMP], normal).rgb;

    float lod = pbr_params.roughness * MAX_REFLECTION_LOD;
    vec3 prefilter_color = sample_texture_cube_lod(per_frame_data.prefilter_map, u_samplers[SAMPLER_LINEAR_CLAMP], reflection, lod).rgb;
    vec3 f0 = vec3(0.04f);

    vec3 diffuse_color = pbr_params.albedo.rgb * (vec3(1.0f) - f0);
    diffuse_color *= (1.0f - pbr_params.metallic);
    vec3 diffuse = diffuse_light * diffuse_color;

    vec3 specular_color = mix(f0, pbr_params.albedo.rgb, pbr_params.metallic);

    vec3 specular = prefilter_color * (specular_color * brdf.x + brdf.y);

    return (diffuse + specular) * ibl_contribution;
}

vec3 evaluateBRDF(vec3 light_direction, vec3 view_dir, vec3 normal, PBRParameter pbr_params) {
    vec3 halfway_vector = normalize(view_dir + light_direction);
    vec3 reflection = normalize(reflect(-view_dir, normal));

    float ndotl = clamp(dot(normal, light_direction), 0.001, 1.0);
    float ndotv = clamp(dot(normal, view_dir), 0.001, 1.0);
    float ndoth = clamp(dot(normal, halfway_vector), 0.0, 1.0);
    float ldoth = clamp(dot(light_direction, halfway_vector), 0.0, 1.0);

    // Directional Light Lighting calculation
    vec3 Lo = vec3(0.0f);
    vec3 F0 = mix(vec3(0.04), pbr_params.albedo.rgb, pbr_params.metallic);
    vec3 diffuse = pbr_params.albedo.rgb / PI;

    float D = D_GGX(ndoth, pbr_params.roughness);
    float G = G_Smith(ndotv, ndotl, pbr_params.roughness);

    vec3 F = F_Schlick(ldoth, F0);
    vec3 specular = (D * F * G) / (4.0 * ndotv * ndotl + 0.0001);

    vec3 kD = (1.0 - specular) * (1.0 - pbr_params.metallic);
    return (kD * diffuse + specular) * ndotl;
}

vec3 evaluateDirectionalLight(in Light light, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.direction;
    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance;
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
    float attenuation = getSquareFallOffAttenaution(distance2, light.radius_or_height);
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance * attenuation;
}

vec3 evaluateSpotLight(in Light light, in vec3 world_pos, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.position - world_pos;
    float distance2 = dot(light_direction, light_direction);
    light_direction /= sqrt(distance2);

    vec3 radiance = u32_to_rgba(light.color).rgb * light.intensity;
    float attenuation = getSquareFallOffAttenaution(distance2, light.radius_or_height);
    attenuation *= getAngularAttenuation(-light_direction, light.direction, light.inner_angle, light.outer_angle);
    return evaluateBRDF(light_direction, view_dir, normal, pbr_params) * shadow_factor * radiance * attenuation;
}

#endif