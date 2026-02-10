#version 460
#extension GL_GOOGLE_include_directive : enable

#include "utils/per-frame-data.glsl"
#include "utils/color.glsl"

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

#include "utils/pbr-lighting.glsl"

layout(set = 3, binding = 1) readonly buffer MaterialBinding {
    PBRMaterial materials[];
};

layout(push_constant) uniform PushConstantData {
    float debug_texture_index;
    float split_percentage;
    uint _padding[2];
};

PBRParameter get_pbr_material_parameter(PBRMaterial material, vec2 uv) {
    PBRParameter out_params;
    out_params.albedo = material.albedo;
    out_params.emissive = material.emissive_factor;

    if (material.albedo_texture != K_INVALID_TEXTURE) {
        vec4 albedo = sample_texture(material.albedo_texture, uv);
        albedo.rgb = srgb_to_linear(albedo.rgb);
        out_params.albedo *= albedo;
    }

    if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
        vec2 mr = sample_texture(material.metallic_roughness_texture, uv).bg;
        out_params.metallic = mr.x;
        out_params.roughness = mr.y;
    } else {
        out_params.metallic = material.metallic_factor;
        out_params.roughness = material.roughness_factor;
    }

    out_params.roughness = is_specular_glossiness_workflow(material.flags) ? 1.0 - out_params.roughness : out_params.roughness;

    if (material.emissive_texture != K_INVALID_TEXTURE)
        out_params.emissive *= srgb_to_linear(sample_texture(material.emissive_texture, uv).rgb);

    return out_params;
}

void main() {
    vec3 col = vec3(0.0f);
    vec3 normal = vec3(0.0f, 0.0f, 1.0f);

    PBRMaterial material = materials[fs_in.mat_id];
    if (material.normal_texture != K_INVALID_TEXTURE)
        normal = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    PBRParameter pbr_params = get_pbr_material_parameter(material, fs_in.uv);
    pbr_params.ao = 1.0f;

    vec3 view_dir = normalize(per_frame_data.camera_position.xyz - fs_in.world_pos);

    Light light;
    light.direction_or_position = per_frame_data.light_direction;
    light.cast_shadow = per_frame_data.cast_shadow;
    light.color = per_frame_data.light_color;
    light.intensity = per_frame_data.light_intensity;

    float shadow_factor = 1.0f;
    vec3 Lo;
    if (split_percentage > gl_FragCoord.x) {
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
        Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor);
    }
    fragColor = vec4(Lo, 1.0f);
}
