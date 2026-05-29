#version 460

#extension GL_GOOGLE_include_directive : enable

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
    vec2 velocity = get_pixel_velocity(fs_in.current_clip_pos, fs_in.prev_clip_pos, per_frame_data.current_frame_jitter, per_frame_data.prev_frame_jitter);
    velocity_buffer = velocity;

    frag_color = vec4(1.0f, 0.0f, 0.5f, 1.0f);
}
