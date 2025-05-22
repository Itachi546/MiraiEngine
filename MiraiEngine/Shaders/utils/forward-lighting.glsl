layout(location = 0) out vec4 fragColor;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec2 uv;
    flat uint mat_id;
}
fs_in;

#include "utils/bindless.glsl"
#include "utils/material.glsl"
#include "pbr.glsl"

layout(set = 3, binding = 1) readonly buffer Materials {
    Material materials[];
};

layout(push_constant) uniform PushConstant {
    layout(offset = 16) mat4 invVP;
    vec4 camera_position;
    vec4 light_direction;
    vec4 light_color;
    uint irradiance_map;
    uint prefilter_map;
    uint brdf_texture;
};

void main() {
    vec3 col = vec3(0.0f);

    Material material = materials[fs_in.mat_id];
    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE)
        albedo *= sample_texture(material.albedo_texture, fs_in.uv);

#if FORWARD_TRANSPARENT_PASS
    if (albedo.a < 0.5)
        discard;
#endif

    vec3 normal = vec3(0.0f, 0.0f, 1.0f);
    if (material.normal_texture != K_INVALID_TEXTURE)
        normal = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    float metallic = material.metallic_factor;
    float roughness = material.roughness_factor;
    if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
        vec2 mr = sample_texture(material.metallic_roughness_texture, fs_in.uv).bg;
        metallic = mr.x;
        roughness = mr.y;
    }

    vec3 emissive = material.emissive_factor;
    if (material.emissive_texture != K_INVALID_TEXTURE)
        emissive *= sample_texture(material.emissive_texture, fs_in.uv).rgb;

    vec3 view_dir = normalize(camera_position.xyz - fs_in.world_pos);
    vec3 halfway_vector = normalize(view_dir + light_direction.xyz);

    float ndotl = max(dot(normal, light_direction.xyz), 0.0);
    float ndotv = max(dot(normal, view_dir), 0.0);
    float ndoth = max(dot(normal, halfway_vector), 0.0);
    float ldoth = max(dot(light_direction.xyz, halfway_vector), 0.0);
    float ao = 0.8f;

    // Directional Light Lighting calculation
    vec3 Lo = vec3(0.0f);
    float shadow_factor = 1.0f;
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    {
        vec3 diffuse = albedo.rgb / PI;

        float D = D_GGX(ndoth, roughness);
        float G = G_Smith(ndotv, ndotl, roughness);

        vec3 F = F_Schlick(ldoth, F0);
        vec3 specular = (D * F * G) / (4.0 * ndotv * ndotl + 0.0001);

        // For directional light
        vec3 radiance = light_color.xyz * light_color.w;
        vec3 kD = (1.0 - F) * (1.0 - metallic);

        // Apply AO to direction light too
        Lo += (kD * diffuse + specular) * shadow_factor * radiance * ndotl * ao;
    }

    vec3 Ks = F_SchlickRoughness(ndotv, F0, roughness);
    vec3 Kd = (1.0 - Ks) * (1.0 - metallic);
    vec3 irradiance = sample_texture_cube(irradiance_map, normal).rgb;
    vec3 diffuse = irradiance * albedo.rgb;
    vec3 R = reflect(-view_dir, normal);
    vec3 prefilter_color = sample_texture_cube_lod(prefilter_map, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = sample_texture(brdf_texture, vec2(ndotv, roughness)).rg;
    vec3 specular = prefilter_color * (Ks * brdf.x + brdf.y);

    vec3 ambient = (Kd * diffuse + specular) * ao;
    Lo += ambient + emissive;
    fragColor = vec4(Lo, 1.0f);
}