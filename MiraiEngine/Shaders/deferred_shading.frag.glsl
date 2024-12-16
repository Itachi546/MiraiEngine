#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/transform.glsl"
#include "utils/shadow.glsl"
layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec2 uv;

layout(set = 0, binding = 0) uniform sampler2D albedo_texture;
layout(set = 0, binding = 1) uniform sampler2D depth_texture;
layout(set = 0, binding = 2) uniform sampler2D normal_pbr_texture;
layout(set = 0, binding = 3) uniform sampler2D emissive_texture;
layout(set = 0, binding = 4) uniform sampler2DArray shadow_depth_texture;

layout(set = 1, binding = 0) uniform CascadeInfo {
    mat4 VP[5];
    vec4 split_distances[2];
};

layout(push_constant) uniform PushConstant {
    mat4 invVP;
    vec4 camera_position;
    vec3 light_direction;
};

int select_cascade_index(float cam_dist) {
    float cascade_span = split_distances[1][1];
    if (cam_dist < split_distances[0][0] * cascade_span)
        return 0;
    if (cam_dist < split_distances[0][1] * cascade_span)
        return 1;
    if (cam_dist < split_distances[0][2] * cascade_span)
        return 2;
    if (cam_dist < split_distances[0][3] * cascade_span)
        return 3;
    if (cam_dist < split_distances[1][0] * cascade_span)
        return 4;

    /*
    for (int i = 0; i < 5; ++i) {
        int k = i / 4;
        int c = i % 4;
        if (cam_dist < split_distances[k][c] * cascade_span)
            return i;
    }
    */
    return -1;
}

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

    float specular = 0.0f; //pow(ndoth, 64.0f);

    vec3 col = vec3(0.0f);
    int cascade_index = select_cascade_index(cam_dist);

#if 1
    col = cascade_index == -1 ? vec3(1.0) : CASCADE_COLORS[cascade_index];
#endif

    //col += (diffuse + 0.01) * albedo.rgb + specular + emissive;
    fragColor = vec4(col, 1.0f);
}