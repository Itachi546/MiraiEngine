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
};

bool is_valid(uint texture) {
    return texture != K_INVALID_TEXTURE;
}

void main() {
    PBRMaterial material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (is_valid(material.albedo_texture)) {
        vec4 col = sample_texture(material.albedo_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv);
        col.rgb = srgb_to_linear(col.rgb);
        albedo *= col;
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
        pbr_params.emissive *= srgb_to_linear(sample_texture(material.emissive_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).rgb);

    if (is_valid(material.metallic_roughness_texture)) {
        vec2 mr = sample_texture(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv).bg;
        pbr_params.metallic = mr.x;
        pbr_params.roughness = mr.y;
    } else {
        pbr_params.metallic = material.metallic_factor;
        pbr_params.roughness = material.roughness_factor;
    }
    pbr_params.roughness = is_specular_glossiness_workflow(material.flags) ? 1.0 - pbr_params.roughness : pbr_params.roughness;

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
    if (light.cast_shadow > 0.5) {
        int cascade_index = 0;
        shadow_factor = max(calculate_shadow_factor(fs_in.world_pos + normal * 0.001f, cam_dist, cascade_index), 0.0f);
    }

    // Debug Params
    vec3 Lo;
    if (split_percentage >= screen_uv.x) {
        if (debug_texture_index > 4.5f)
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
        Lo = linear_to_srgb(ACESFilm(calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor + 0.05f)));
    }
    fragColor = vec4(Lo, 1.0f);
}

#endif