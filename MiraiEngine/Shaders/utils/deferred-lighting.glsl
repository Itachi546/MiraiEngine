#include "transform.glsl"
#include "shadow.glsl"
#include "pbr.glsl"
#include "bindless.glsl"
#include "color.glsl"
#include "per-frame-data.glsl"

layout(location = 0) out vec4 fragColor;
layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;
layout(set = 0, binding = 3) uniform sampler2D emissive_texture;
layout(set = 0, binding = 4) uniform sampler2D ssao_texture;

layout(set = 2, binding = 0) uniform PerFrameBindings {
    PerFrameData per_frame_data;
};

#if ENABLE_RT_SHADOW
layout(set = 0, binding = 5) uniform sampler2D shadow_texture;
#else
layout(set = 0, binding = 5) uniform sampler2D shadow_depth_texture;
layout(set = 2, binding = 1) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};
#include "utils/directional-shadow.glsl"
#endif

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0 - 1.0, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, per_frame_data.invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);
    vec3 emissive = textureLod(emissive_texture, uv, 0).rgb;

    vec3 normal = octahedral_decode(normal_pbr.xy * 2.0 - 1.0);
    float metallic = normal_pbr.z;

    float roughness = normal_pbr.w;

    vec3 view_dir = per_frame_data.camera_position.xyz - world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    vec3 halfway_vector = normalize(view_dir + per_frame_data.light_direction.xyz);

    float ndotl = max(dot(normal, per_frame_data.light_direction.xyz), 0.0);
    float ndotv = max(dot(normal, view_dir), 0.0);
    float ndoth = max(dot(normal, halfway_vector), 0.0);
    float ldoth = max(dot(per_frame_data.light_direction.xyz, halfway_vector), 0.0);
    float ao = texture(ssao_texture, uv).r;

    float shadow_factor = 1.0f;
    if (per_frame_data.cast_shadow > 0.5f) {
#if ENABLE_RT_SHADOW
        shadow_factor = texture(shadow_texture, uv).r;
#else
        int cascade_index = 0;
        shadow_factor = max(calculate_shadow_factor(world_pos, cam_dist, cascade_index), 0.0f);
#endif
    }
    // Directional Light Lighting calculation
    vec3 Lo = vec3(0.0f);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
    {
        vec3 diffuse = albedo.rgb / PI;

        float D = D_GGX(ndoth, roughness);
        float G = G_Smith(ndotv, ndotl, roughness);

        vec3 F = F_Schlick(ldoth, F0);
        vec3 specular = (D * F * G) / (4.0 * ndotv * ndotl + 0.0001);

        // For directional light
        vec3 radiance = per_frame_data.light_color.xyz * per_frame_data.light_intensity;
        vec3 kD = (1.0 - F) * (1.0 - metallic);

        // Apply AO to direction light too
        Lo += (kD * diffuse * ao + specular * ao) * shadow_factor * radiance * ndotl;
    }

    vec3 Ks = F_SchlickRoughness(ndotv, F0, roughness);
    vec3 Kd = (1.0 - Ks) * (1.0 - metallic);
    vec3 irradiance = sample_texture_cube(per_frame_data.irradiance_map, normal).rgb;
    vec3 diffuse = irradiance * albedo.rgb;
    vec3 R = reflect(-view_dir, normal);
    vec3 prefilter_color = sample_texture_cube_lod(per_frame_data.prefilter_map, R, roughness * MAX_REFLECTION_LOD).rgb;
    vec2 brdf = sample_texture(per_frame_data.brdf_texture_map, vec2(ndotv, roughness)).rg;
    vec3 specular = prefilter_color * (Ks * brdf.x + brdf.y);

    vec3 ambient = (Kd * diffuse + specular);
    Lo += ambient + emissive;
    fragColor = vec4(Lo, 1.0f);
}