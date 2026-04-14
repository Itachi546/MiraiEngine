#ifndef FORWARD_PASS_GLSL
#define FORWARD_PASS_GLSL

#include "../utils/per-frame-data.glsl"
#include "../utils/shadow.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"

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

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

#include "../pbr/pbr-lighting.glsl"

layout(set = 0, binding = 4) readonly buffer Materials {
    PBRMaterial materials[];
};

layout(set = 0, binding = 5) uniform texture2D u_ssao_texture;

layout(set = 0, binding = 6) uniform texture2D u_shadow_texture;

layout(set = 0, binding = 7) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

#include "../shadow/directional-shadow.glsl"

layout(push_constant) uniform PushConstants {
    float split_percentage;
    float debug_texture_index;
    float pcf_radius;
    float pcf_sample_count;
    float ibl_intensity;
    float _padding[3];
};

bool is_valid(uint texture) {
    return texture != K_INVALID_TEXTURE;
}

void main() {
    PBRMaterial material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (is_valid(material.albedo_texture)) {
        albedo *= sample_texture(material.albedo_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv);
    }
#ifdef ALPHA_MODE_MASK
    if (albedo.a <= material.alpha_cutoff)
        discard;
#endif
    vec3 normal = vec3(0.0f, 0.0f, 1.0f);
    if (is_valid(material.normal_texture))
        normal = sample_texture(material.normal_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).rgb * 2.0f - 1.0f;
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    PBRParameter pbr_params;
    pbr_params.albedo = albedo;
    pbr_params.emissive = material.emissive_factor;

    if (is_valid(material.emissive_texture))
        pbr_params.emissive *= sample_texture(material.emissive_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).rgb;

    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (is_specular_glossiness_workflow(material.flags)) {
        vec4 specular_glossiness = metallic_roughness.rrrg;
        if (material.metallic_roughness_texture != K_INVALID_TEXTURE)
            specular_glossiness.rgb = sample_texture(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).rgb;

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
        if (material.metallic_roughness_texture != K_INVALID_TEXTURE)
            metallic_roughness = sample_texture(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).bg;
    }

    pbr_params.metallic = metallic_roughness.x;
    pbr_params.roughness = metallic_roughness.y;

    vec2 screen_uv = gl_FragCoord.xy / vec2(per_frame_data.width, per_frame_data.height);
    pbr_params.ao = texture(sampler2D(u_ssao_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), screen_uv).r;

    vec3 view_dir = per_frame_data.camera_position.xyz - fs_in.world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    Light light;
    light.direction_or_position = per_frame_data.light_direction;
    light.cast_shadow = per_frame_data.cast_shadow;
    light.color = per_frame_data.light_color;
    light.intensity = per_frame_data.light_intensity;

    float shadow_factor = 1.0f;
    int cascade_index = 0;
    if (light.cast_shadow > 0.5) {
        shadow_factor = max(calculate_shadow_factor(fs_in.world_pos, cam_dist, cascade_index, pcf_radius, pcf_sample_count), 0.0f);
    }

    // Debug Params
    vec3 Lo;
    if (split_percentage >= screen_uv.x) {
        if (debug_texture_index > 5.5) {
            Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor + 0.05f, ibl_intensity);
            Lo *= light.cast_shadow > 0.5 ? get_cascade_debug_color(fs_in.world_pos + normal * 0.001f, cam_dist, cascade_index) : vec3(1.0f);
        } else if (debug_texture_index > 4.5f)
            Lo = vec3(shadow_factor);
        else if (debug_texture_index > 3.5f)
            Lo = vec3(pbr_params.ao);
        else if (debug_texture_index > 2.5f)
            Lo = vec3(pbr_params.roughness);
        else if (debug_texture_index > 1.5f)
            Lo = vec3(pbr_params.metallic);
        else if (debug_texture_index > 0.5f)
            Lo = normal * 0.5 + 0.5;
        else
            Lo = pbr_params.albedo.xyz;
    } else {
        Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor + 0.05f, ibl_intensity);
    }

    fragColor = vec4(Lo, 1.0f);
}

#endif