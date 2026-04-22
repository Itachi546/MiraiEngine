#ifndef FORWARD_PASS_GLSL
#define FORWARD_PASS_GLSL

#include "../utils/per-frame-data.glsl"
#include "../utils/shadow.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/debug-options.glsl"

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

#include "../utils/light.glsl"
#include "../pbr/pbr-lighting.glsl"

layout(std430, set = 0, binding = 4) readonly buffer Materials {
    PBRMaterial materials[];
};

layout(set = 0, binding = 5) uniform texture2D u_ssao_texture;

layout(set = 0, binding = 6) uniform texture2D u_shadow_texture;

layout(set = 0, binding = 7) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

layout(std430, set = 0, binding = 8) readonly buffer Lights {
    Light lights[];
};

layout(std430, set = 0, binding = 9) readonly buffer LightLists {
    uint light_lists[];
};

#include "../shadow/directional-shadow.glsl"

layout(push_constant) uniform PushConstants {
    float split_percentage;
    float debug_texture_index;
    float pcf_radius;
    float pcf_sample_count;

    float ibl_intensity;
    float num_lights;
    float light_culling;
    float tile_size;
};

bool is_valid(uint texture) {
    return texture != K_INVALID_TEXTURE;
}

void main() {
    PBRMaterial material = materials[fs_in.mat_id];

    vec2 texture_scale = vec2(material.texture_scale_x, material.texture_scale_y);

    vec4 albedo = material.albedo;
    if (is_valid(material.albedo_texture)) {
        albedo *= sample_texture(material.albedo_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv * texture_scale);
    }
#ifdef ALPHA_MODE_MASK
    if (albedo.a <= material.alpha_cutoff)
        discard;
#endif
    vec3 normal = vec3(0.0f, 0.0f, 1.0f);
    if (is_valid(material.normal_texture))
        normal = sample_texture(material.normal_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv * texture_scale).rgb * 2.0f - 1.0f;
    normal = normalize(normal.x * fs_in.tangent + normal.y * fs_in.bitangent + normal.z * fs_in.normal);

    PBRParameter pbr_params;
    pbr_params.albedo = albedo;
    pbr_params.emissive = material.emissive_factor;

    if (is_valid(material.emissive_texture))
        pbr_params.emissive *= sample_texture(material.emissive_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv * texture_scale).rgb;

    vec2 metallic_roughness = vec2(material.metallic_factor, material.roughness_factor);
    if (is_specular_glossiness_workflow(material.flags)) {
        vec4 specular_glossiness = metallic_roughness.rrrg;
        if (material.metallic_roughness_texture != K_INVALID_TEXTURE)
            specular_glossiness.rgb = sample_texture(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv * texture_scale).rgb;

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
            metallic_roughness = sample_texture(material.metallic_roughness_texture, u_samplers[SAMPLER_LINEAR_REPEAT], fs_in.uv * texture_scale).bg;
    }

    pbr_params.metallic = metallic_roughness.x;
    pbr_params.roughness = metallic_roughness.y;

    vec2 screen_uv = gl_FragCoord.xy / vec2(per_frame_data.width, per_frame_data.height);
    pbr_params.ao = texture(sampler2D(u_ssao_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), screen_uv).r;

    vec3 view_dir = per_frame_data.camera_position.xyz - fs_in.world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    vec3 Lo = vec3(0.0f);
    bool dir_light_cast_shadow = false;
    int cascade_index = 0;
    float shadow_factor = 1.0f;
    uint tile_light_count = 0;

    if (light_culling < 0.5) {
        for (int i = 0; i < int(num_lights); ++i) {
            Light light = lights[i];
            uint light_type = get_light_type(light.flag);
            if (light_type == LIGHT_TYPE_DIRECTIONAL) {
                if (cast_shadow(light.flag)) {
                    dir_light_cast_shadow = true;
                    shadow_factor = max(calculate_shadow_factor(fs_in.world_pos, cam_dist, cascade_index, pcf_radius, pcf_sample_count), 0.05f);
                }
                Lo += evaluateDirectionalLight(light, view_dir, normal, pbr_params, shadow_factor);
            } else if (light_type == LIGHT_TYPE_POINT) {
                Lo += evaluatePointLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
            } else if (light_type == LIGHT_TYPE_SPOT) {
                Lo += evaluateSpotLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
            }
        }
    } else {
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
                if (cast_shadow(light.flag)) {
                    dir_light_cast_shadow = true;
                    shadow_factor = max(calculate_shadow_factor(fs_in.world_pos, cam_dist, cascade_index, pcf_radius, pcf_sample_count), 0.05f);
                }
                Lo += evaluateDirectionalLight(light, view_dir, normal, pbr_params, shadow_factor);
            } else if (light_type == LIGHT_TYPE_POINT) {
                Lo += evaluatePointLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
            } else if (light_type == LIGHT_TYPE_SPOT) {
                Lo += evaluateSpotLight(light, fs_in.world_pos, view_dir, normal, pbr_params, 1.0f);
            }
        }
    }

    // Debug Params
    if (split_percentage >= screen_uv.x) {
        uint debug_index = uint(debug_texture_index);
        switch (debug_index) {
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
        };
    }

    Lo += pbr_params.emissive;

#ifdef ALPHA_MODE_TRANSPARENT
    // @TODO temp
    fragColor = vec4(Lo, albedo.a);
#else
    fragColor = vec4(Lo, 1.0f);
#endif
}

#endif