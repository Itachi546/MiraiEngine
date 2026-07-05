#ifndef FORWARD_PASS_GLSL
#define FORWARD_PASS_GLSL

#include "../utils/per-frame-data.glsl"
#include "../utils/shadow.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/debug-options.glsl"

layout(location = 0) out vec4 frag_color;
layout(location = 1) out vec2 velocity_buffer;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 world_pos;
    vec2 uv;
    flat uint mat_id;
    vec4 current_clip_pos;
    vec4 prev_clip_pos;
    flat uint draw_id;
}
fs_in;

layout(set = 0, binding = 0) uniform PerFrameBinding {
    PerFrameData per_frame_data;
};

#include "../utils/light.glsl"

#define ENABLE_INDIRECT_LIGHTING
#include "../pbr/pbr-lighting.glsl"

layout(std430, set = 0, binding = 4) readonly buffer Materials {
    PBRMaterial materials[];
};

layout(set = 0, binding = 5) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

#include "../shadow/directional-shadow.glsl"

layout(std430, set = 0, binding = 6) readonly buffer Lights {
    Light lights[];
};

layout(std430, set = 0, binding = 7) readonly buffer LightLists {
    uint light_lists[];
};

layout(push_constant) uniform PushConstants {
    float split_percentage;
    int debug_texture_index;
    float pcf_radius;
    float pcf_sample_count;

    float ibl_intensity;
    uint num_lights;
    float disable_punctual_light;
    uint tile_size;

    float mip_bias;
    uint ssao_texture_index;
    uint shadow_texture_index;
    uint _padding;
};

void main() {
    PBRMaterial material = materials[fs_in.mat_id];

    vec2 texture_scale = vec2(material.texture_scale_x, material.texture_scale_y);

    vec2 scaled_uv = fs_in.uv * texture_scale;
    vec4 albedo = fetch_albedo(material, scaled_uv, mip_bias);

#ifdef ALPHA_MODE_MASK
    if (albedo.a <= material.alpha_cutoff)
        discard;
#endif
    vec3 normal = fetch_normal_map(material, scaled_uv, mip_bias);
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    PBRParameter pbr_params;
    pbr_params.albedo = albedo;
    pbr_params.emissive = fetch_emissive(material, scaled_uv, mip_bias);

    vec2 metallic_roughness = fetch_pbr_metallic_roughness(material, scaled_uv, mip_bias);
    pbr_params.metallic = metallic_roughness.x;
    pbr_params.roughness = metallic_roughness.y;

    vec2 screen_uv = gl_FragCoord.xy / vec2(per_frame_data.width, per_frame_data.height);
    if (is_valid(ssao_texture_index))
        pbr_params.ao = sample_texture(ssao_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], screen_uv).r;
    else
        pbr_params.ao = 1.0f;

    vec3 view_dir = per_frame_data.camera_position.xyz - fs_in.world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    vec3 Lo = vec3(0.0f);
    bool dir_light_cast_shadow = false;
    int cascade_index = 0;
    float shadow_factor = 1.0f;
    uint tile_light_count = 0;

    vec3 reflection = normalize(reflect(-view_dir, normal));
    float ndotv = clamp(dot(normal, view_dir), 0.001, 1.0);
    vec3 F0 = mix(vec3(0.04), pbr_params.albedo.rgb, pbr_params.metallic);
    Lo += getIBLContribution(reflection, normal, ndotv, F0, pbr_params, ibl_intensity);

    ShadowParams shadow_params;
    shadow_params.world_pos = fs_in.world_pos;
    shadow_params.texture_index = shadow_texture_index;
    shadow_params.cam_dist = cam_dist;
    shadow_params.pcf_radius = pcf_radius;
    shadow_params.pcf_sample_count = pcf_sample_count;
    shadow_params.shadow_texel_size = 1.0f / cascade_info.cascade_texture_size;

    uvec2 resolution = uvec2(per_frame_data.width, per_frame_data.height);
    uvec2 tile_count = resolution / uvec2(tile_size);
    uvec2 tile_id = uvec2(gl_FragCoord.xy) / uvec2(tile_size);

    uint tile_index = get_tile_address_opaque(tile_id, tile_count);

    tile_light_count = light_lists[tile_index++];
    for (int i = 0; i < tile_light_count; ++i) {
        uint light_index = light_lists[tile_index + i];
        Light light = lights[light_index];
        uint light_type = get_light_type(light.flag);
        if (light_type == LIGHT_TYPE_DIRECTIONAL) {
            if (cast_shadow(light.flag) && is_valid(shadow_texture_index)) {
                dir_light_cast_shadow = true;
                shadow_factor = max(calculate_shadow_factor(shadow_params, cascade_index), 0.05f);
            }
            Lo += evaluateDirectionalLight(light, view_dir, normal, pbr_params, shadow_factor);
        } else if (light_type == LIGHT_TYPE_POINT && disable_punctual_light < 0.5f) {
            Lo += evaluatePointLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
        } else if (light_type == LIGHT_TYPE_SPOT && disable_punctual_light < 0.5f) {
            Lo += evaluateSpotLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
        }
    }

    Lo += pbr_params.emissive * 5.0f;

    vec2 velocity = get_pixel_velocity(fs_in.current_clip_pos, fs_in.prev_clip_pos, per_frame_data.current_frame_jitter, per_frame_data.prev_frame_jitter);
    // Debug Params
    if (split_percentage >= screen_uv.x) {
        switch (debug_texture_index) {
        case DEBUG_ALBEDO:
            Lo = pbr_params.albedo.xyz;
            break;
        case DEBUG_NORMAL:
            Lo = normal * 0.5 + 0.5;
            break;
        case DEBUG_METALLIC:
            Lo = vec3(pbr_params.metallic);
            break;
        case DEBUG_ROUGHNESS:
            Lo = vec3(pbr_params.roughness);
            break;
        case DEBUG_AO:
            Lo = vec3(pbr_params.ao);
            break;
        case DEBUG_SHADOW:
            Lo = vec3(shadow_factor);
            break;
        case DEBUG_CSM_SPLIT:
            Lo *= dir_light_cast_shadow ? get_cascade_debug_color(fs_in.world_pos + normal * 0.001f, cam_dist, cascade_index) : vec3(1.0f);
            break;
        case DEBUG_LIGHT_TILE:
            Lo = get_tile_heatmap(tile_light_count, 50);
            break;
        case DEBUG_VELOCITY:
            Lo = vec3(velocity, 0.0f) * 100.0f;
            break;
        };
    }

#ifdef ALPHA_MODE_TRANSPARENT
    frag_color = vec4(Lo, albedo.a);
#else
    frag_color = vec4(Lo, 1.0f);
#endif
    velocity_buffer = velocity;
}

#endif