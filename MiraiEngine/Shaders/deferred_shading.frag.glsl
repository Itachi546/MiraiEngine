#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"
#include "utils/shadow.glsl"
#include "utils/pbr.glsl"
#include "utils/bindless.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;
layout(set = 0, binding = 3) uniform sampler2D emissive_texture;
layout(set = 0, binding = 4) uniform sampler2DArray shadow_depth_texture;
layout(set = 0, binding = 5) uniform sampler2D ssao_texture;

layout(set = 2, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

#include "utils/directional-shadow.glsl"

layout(push_constant) uniform PushConstant {
    mat4 invVP;
    vec4 camera_position;
    vec4 light_direction;
    uint irradiance_map;
    uint prefilter_map;
    uint brdf_texture;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0 - 1.0, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);
    vec3 emissive = textureLod(emissive_texture, uv, 0).rgb;

    vec3 normal = octahedral_decode(normal_pbr.xy * 2.0 - 1.0);
    float metallic = normal_pbr.z;
    float roughness = normal_pbr.w;

    vec3 view_dir = camera_position.xyz - world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    vec3 halfway_vector = normalize(view_dir + light_direction.xyz);

    float ndotl = max(dot(normal, light_direction.xyz), 0.0);
    float ndotv = max(dot(normal, view_dir), 0.0);
    float ndoth = max(dot(normal, halfway_vector), 0.0);
    float ldoth = max(dot(light_direction.xyz, halfway_vector), 0.0);
    // DEBUG SHADOW CASCADE
#if 0
    albedo.rgb = cascade_index == -1 ? vec3(1.0) : u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb;
#endif
    int cascade_index = 0;
    vec3 Lo = vec3(0.0f);
    vec3 F0 = mix(vec3(0.04), albedo.rgb, metallic);
#if 0
    float shadow_factor = max(calculate_shadow_factor(world_pos, cam_dist, cascade_index), 0.0f);
    {
        vec3 diffuse = albedo.rgb / PI;

        float D = D_GGX(ndoth, roughness);
        float G = G_Smith(ndotv, ndotl, roughness);

        vec3 F = F_Schlick(ldoth, F0);
        vec3 specular = (D * F * G) / (4.0 * ndotv * ndotl + 0.0001);

        // For directional light
        vec3 radiance = vec3(1.0f);
        vec3 kD = (1.0 - F) * (1.0 - metallic);
        Lo += (kD * diffuse + specular) * shadow_factor * radiance * ndotl;
    }
#endif
    vec3 Ks = F_SchlickRoughness(ndotv, F0, roughness);
    vec3 Kd = (1.0 - Ks) * (1.0 - metallic);

    float ao = texture(ssao_texture, uv).r;
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