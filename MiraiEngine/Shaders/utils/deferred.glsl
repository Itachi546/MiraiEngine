layout(location = 0) out vec4 albedo_buffer;
layout(location = 1) out vec4 normal_buffer;
layout(location = 2) out vec4 emissive_buffer;
layout(location = 3) out vec2 velocity_buffer;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec3 light_pos;
    vec3 view_dir;
    vec2 uv;
    vec4 current_clip_pos;
    vec4 prev_clip_pos;
    flat uint mat_id;
}
fs_in;

layout(push_constant) uniform PushConstant {
    mat4 last_frame_VP;
    vec2 prev_frame_jitter;
    vec2 current_frame_jitter;
};

#include "utils/bindless.glsl"
#include "utils/material.glsl"
#include "utils/transform.glsl"
#include "utils/color.glsl"

layout(set = 3, binding = 1) readonly buffer Materials {
    PBRMaterial materials[];
};

void main() {
    vec3 col = vec3(0.0f);

    PBRMaterial material = materials[fs_in.mat_id];

    vec4 albedo = material.albedo;
    if (material.albedo_texture != K_INVALID_TEXTURE) {
        vec4 texture_albedo = sample_texture(material.albedo_texture, fs_in.uv);
        texture_albedo.rgb = srgb_to_linear(texture_albedo.rgb);
        albedo *= texture_albedo;
    }

#ifdef ALPHA_PASS
    if (albedo.a <= material.alpha_cutoff)
        discard;
#endif

    albedo_buffer = albedo;

    vec3 n = vec3(0.0f, 0.0f, 1.0f);
    if (material.normal_texture != K_INVALID_TEXTURE)
        n = sample_texture(material.normal_texture, fs_in.uv).rgb * 2.0f - 1.0f;
    // n.xy *= 4.0f;
    n = normalize(n.x * fs_in.tangent + n.y * fs_in.bitangent + n.z * fs_in.normal);
    vec2 oct_n = octahedral_encode(n) * 0.5 + 0.5;

    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (material.metallic_roughness_texture != K_INVALID_TEXTURE) {
        if (is_specular_glossiness_workflow(material.flags)) {
            vec3 spec = srgb_to_linear(sample_texture(material.metallic_roughness_texture, fs_in.uv).rgb);
            float spec_intensity = max(spec.r, max(spec.g, spec.b));
            metallic_roughness.x = clamp((spec_intensity - 0.04) / (1.0 - 0.04), 0.0, 1.0);
            vec3 base_color_dielectric = albedo.xyz / (1.0 - 0.04);
            albedo.xyz = mix(base_color_dielectric, spec, vec3(metallic_roughness.x));
        } else {
            metallic_roughness = sample_texture(material.metallic_roughness_texture, fs_in.uv).bg;
        }
    }

    normal_buffer = vec4(oct_n, metallic_roughness);

    vec3 emissive = material.emissive_factor;
    if (material.emissive_texture != K_INVALID_TEXTURE)
        emissive *= srgb_to_linear(sample_texture(material.emissive_texture, fs_in.uv).rgb);
    emissive_buffer = vec4(emissive, 1.0f);

    vec2 current_ndc_pos = fs_in.current_clip_pos.xy / fs_in.current_clip_pos.w;
    vec2 prev_ndc_pos = fs_in.prev_clip_pos.xy / fs_in.prev_clip_pos.w;

    vec2 velocity = current_ndc_pos - prev_ndc_pos;
    velocity -= (current_frame_jitter - prev_frame_jitter) * 0.5;
    velocity_buffer = velocity;
}