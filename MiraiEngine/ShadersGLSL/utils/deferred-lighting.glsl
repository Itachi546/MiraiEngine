#include "transform.glsl"
#include "shadow.glsl"
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

#include "pbr-lighting.glsl"

#if ENABLE_RT_SHADOW
layout(set = 0, binding = 5) uniform sampler2D shadow_texture;
#else
layout(set = 0, binding = 5) uniform sampler2D shadow_depth_texture;
layout(set = 2, binding = 1) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};
#include "utils/directional-shadow.glsl"
#endif

// Used for debugging texture
layout(push_constant) uniform PushConstants {
    float split_percentage;
    float debug_texture_index;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv.x * 2.0 - 1.0, 1 - 2.0 * uv.y, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, per_frame_data.invVP);

    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);
    vec3 normal = octahedral_decode(normal_pbr.xy * 2.0 - 1.0);

    PBRParameter pbr_params;
    pbr_params.albedo = textureLod(albedo_texture, uv, 0);
    pbr_params.emissive = textureLod(emissive_texture, uv, 0).rgb;
    pbr_params.metallic = normal_pbr.z;
    pbr_params.roughness = normal_pbr.w;
    pbr_params.ao = texture(ssao_texture, uv).r;

    vec3 view_dir = per_frame_data.camera_position.xyz - world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    Light light;
    light.direction_or_position = per_frame_data.light_direction;
    light.cast_shadow = per_frame_data.cast_shadow;
    light.color = per_frame_data.light_color;
    light.intensity = per_frame_data.light_intensity;

    float shadow_factor = 1.0f;

    if (light.cast_shadow > 0.5) {

#if ENABLE_RT_SHADOW
        shadow_factor = texture(shadow_texture, uv).r;
#else
        int cascade_index = 0;
        shadow_factor = max(calculate_shadow_factor(world_pos + normal * 0.001f, cam_dist, cascade_index), 0.0f);
#endif
    }
    vec3 Lo;
    if (split_percentage >= uv.x) {
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
        Lo = calculateDirectionalLightIntensity(light, view_dir, normal, pbr_params, shadow_factor + 0.05f);
    }
    fragColor = vec4(Lo, 1.0f);
}