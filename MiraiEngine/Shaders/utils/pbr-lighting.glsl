#ifndef PBR_LIGHTING_GLSL
#define PBR_LIGHTING_GLSL

#include "material.glsl"
#include "bindless.glsl"
#include "pbr.glsl"

struct Light {
    vec3 direction_or_position;
    float cast_shadow;
    vec3 color;
    float intensity;
};

vec3 calculateDirectionalAmbientContribution(vec3 normal, vec3 view_dir, float ndotv, vec3 F0, PBRParameter pbr_params) {
    vec3 Ks = F_SchlickRoughness(ndotv, F0, pbr_params.roughness);
    vec3 Kd = (1.0 - Ks) * (1.0 - pbr_params.metallic);
    vec3 irradiance = sample_texture_cube(per_frame_data.irradiance_map, normal).rgb;
    vec3 diffuse = irradiance * pbr_params.albedo.rgb / PI;
    vec3 R = normalize(reflect(-view_dir, normal));
    vec3 prefilter_color = sample_texture_cube_lod(per_frame_data.prefilter_map, R, pbr_params.roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = sample_texture(per_frame_data.brdf_texture_map, vec2(ndotv, pbr_params.roughness)).rg;
    vec3 specular = prefilter_color * (Ks * brdf.x + brdf.y);

    vec3 ambient = (Kd * diffuse + specular) * pbr_params.ao;
    return ambient;
}

vec3 calculateDirectionalLightIntensity(in Light light, in vec3 view_dir, in vec3 normal, in PBRParameter pbr_params, float shadow_factor) {
    vec3 light_direction = light.direction_or_position;
    vec3 halfway_vector = normalize(view_dir + light_direction);
    float ndotl = max(dot(normal, light_direction), 0.0);
    float ndotv = max(dot(normal, view_dir), 0.0);
    float ndoth = max(dot(normal, halfway_vector), 0.0);
    float ldoth = max(dot(light_direction, halfway_vector), 0.0);

    // Directional Light Lighting calculation
    vec3 Lo = vec3(0.0f);
    vec3 F0 = mix(vec3(0.04), pbr_params.albedo.rgb, pbr_params.metallic);
    {
        vec3 diffuse = pbr_params.albedo.rgb / PI;

        float D = D_GGX(ndoth, pbr_params.roughness);
        float G = G_Smith(ndotv, ndotl, pbr_params.roughness);

        vec3 F = F_Schlick(ldoth, F0);
        vec3 specular = (D * F * G) / (4.0 * ndotv * ndotl + 0.0001);

        // For directional light
        vec3 radiance = light.color * light.intensity;
        vec3 kD = (1.0 - F) * (1.0 - pbr_params.metallic);

        Lo += (kD * diffuse + specular) * shadow_factor * radiance * ndotl;
    }
    return Lo + calculateDirectionalAmbientContribution(normal, view_dir, ndotv, F0, pbr_params) + pbr_params.emissive;
}
#endif