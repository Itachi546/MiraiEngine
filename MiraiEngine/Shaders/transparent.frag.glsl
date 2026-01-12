#version 460
#extension GL_GOOGLE_include_directive : enable

#include "utils/per-frame-data.glsl"

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

layout(set = 3, binding = 1) readonly buffer Materials {
    PBRMaterial materials[];
};

void main() {
    vec3 col = vec3(0.0f);
    PBRMaterial material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE)
        albedo *= sample_texture(material.albedo_texture, fs_in.uv);

    if (albedo.a < 0.5)
        discard;

    PBRParameter pbr_params;
    pbr_params.albedo = albedo;
    pbr_params.emissive = material.emissive_factor;

    if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
        vec2 mr = sample_texture(material.metallic_roughness_texture, fs_in.uv).bg;
        pbr_params.metallic = mr.x;
        pbr_params.roughness = mr.y;
    } else {
        pbr_params.metallic = material.metallic_factor;
        pbr_params.roughness = material.roughness_factor;
    }

    pbr_params.roughness = is_specular_glossiness_workflow(material.flags) ? 1.0 - pbr_params.roughness : pbr_params.roughness;

    if (material.emissive_texture != K_INVALID_TEXTURE)
        pbr_params.emissive *= sample_texture(material.emissive_texture, fs_in.uv).rgb;

    pbr_params.ao = 1.0f;

    vec3 normal = vec3(0.0f, 0.0f, 1.0f);
    if (material.normal_texture != K_INVALID_TEXTURE)
        normal = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    vec3 view_dir = normalize(per_frame_data.camera_position.xyz - fs_in.world_pos);

    Light light;
    light.direction_or_position = per_frame_data.light_direction;
    light.cast_shadow = per_frame_data.cast_shadow;
    light.color = per_frame_data.light_color;
    light.intensity = per_frame_data.light_intensity;

    float shadow_factor = 1.0f;
    vec3 Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor);
    fragColor = vec4(Lo, 1.0f);
}