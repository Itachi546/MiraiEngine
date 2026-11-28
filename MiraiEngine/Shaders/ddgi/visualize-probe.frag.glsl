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
    vec3 center;
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

#include "../ddgi/ddgi.glsl"
#include "../utils/light.glsl"
#include "../pbr/pbr-lighting.glsl"
#include "../utils/transform.glsl"

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
    uint ddgi_irradiance_texture_index;
};

#extension GL_EXT_samplerless_texture_functions : enable

const vec3 PROBE_COUNTS = vec3(32, 8, 16);
const vec3 BOX_MIN = vec3(-15.75f, -1.5f, -10.0f);
const vec3 BOX_MAX = vec3(14.75f, 11.5f, 9.0f);

ivec3 get_probe_grid_index(vec3 world_pos) {
    vec3 spacing = (BOX_MAX - BOX_MIN) / (PROBE_COUNTS - 1.0f);
    vec3 grid_index = round((world_pos - BOX_MIN) / spacing);
    return clamp(ivec3(grid_index), ivec3(0), ivec3(PROBE_COUNTS) - 1);
}

uint hash(uvec3 p) {
    p = p * 1664525u + 1013904223u;

    uint n = p.x ^ p.y ^ p.z;
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    n ^= n >> 16;

    return n;
}

vec3 grid_index_to_color(ivec3 index) {
    uint h = hash(uvec3(index));

    return vec3(
               float((h >> 0) & 255u),
               float((h >> 8) & 255u),
               float((h >> 16) & 255u)) /
           255.0;
}

void main() {
    vec2 velocity = get_pixel_velocity(fs_in.current_clip_pos, fs_in.prev_clip_pos, per_frame_data.current_frame_jitter, per_frame_data.prev_frame_jitter);
    velocity_buffer = velocity;

    ivec3 grid_index = get_probe_grid_index(fs_in.center);
    vec3 normal = normalize(fs_in.normal);
    vec2 oct = octahedral_encode(normal);

    vec2 texel = vec2(get_tile_origin(grid_index, int(PROBE_COUNTS.x))) + 1.0 + oct * 8.0;
    vec2 uv = texel / vec2(texture_size(ddgi_irradiance_texture_index, 0));

    vec3 col = sample_texture(ddgi_irradiance_texture_index, u_samplers[SAMPLER_LINEAR_REPEAT], uv).rgb;

    frag_color = vec4(col, 1.0f);
}
