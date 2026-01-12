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

layout(set = 3, binding = 1) readonly buffer MaterialBinding {
    PBRMaterial materials[];
};

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
    vec3 Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor);
    fragColor = vec4(Lo, 1.0f);
}
