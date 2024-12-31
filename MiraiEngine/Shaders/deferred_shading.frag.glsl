#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"
#include "utils/color.glsl"
#include "utils/shadow.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;
layout(set = 0, binding = 3) uniform sampler2D emissive_texture;
layout(set = 0, binding = 4) uniform sampler2DArray shadow_depth_texture;

layout(set = 1, binding = 0) uniform CascadeInfoUniform {
    CascadeInfo cascade_info;
};

#include "utils/directional-shadow.glsl"

layout(push_constant) uniform PushConstant {
    mat4 invVP;
    vec4 camera_position;
    vec3 light_direction;
};

void main() {
    float depth = textureLod(depth_texture, uv, 0).r;
    vec3 clip_pos = vec3(uv * 2.0 - 1.0, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);
    vec4 albedo = textureLod(albedo_texture, uv, 0);
    vec4 normal_pbr = textureLod(normal_pbr_texture, uv, 0);
    vec3 emissive = textureLod(emissive_texture, uv, 0).rgb;

    vec3 normal = octahedral_decode(normal_pbr.xy * 2.0 - 1.0);
    vec2 metallic_roughness = normal_pbr.zw;

    float diffuse = max(dot(normal, light_direction.xyz), 0.0f);

    vec3 view_dir = camera_position.xyz - world_pos;
    float cam_dist = length(view_dir);
    view_dir /= cam_dist;

    vec3 halfway_vector = normalize(view_dir + light_direction.xyz);
    float ndoth = max(dot(normal, halfway_vector), 0.0);

    float specular = 0.0f; // pow(ndoth, 64.0f);
    int cascade_index = 0;
    float shadow_factor = max(calculate_shadow_factor(world_pos, cam_dist, cascade_index), 0.05f);
#if 0
    albedo.rgb = cascade_index == -1 ? vec3(1.0) : u32_to_rgba(CASCADE_COLORS[cascade_index]).rgb;
#endif
    vec3 col = (diffuse * shadow_factor + 0.05f) * albedo.rgb + specular + emissive;
    fragColor = vec4(col, 1.0f);
}