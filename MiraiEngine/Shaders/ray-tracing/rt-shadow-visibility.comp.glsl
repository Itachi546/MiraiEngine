#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_ray_query : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/light.glsl"
#include "../utils/noise.glsl"

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, r8) uniform writeonly image2D u_visibility_texture;
layout(binding = 1) uniform accelerationStructureEXT tlas;

layout(push_constant) uniform PushConstantData {
    mat4 inv_V;

    vec2 resolution;
    vec2 inv_resolution;

    // Only support point and directional light for now
    vec3 direction_or_position;
    float cos_angular_radius; // cos_max_theta

    float a; //  f / (f - n)
    float b; // (f * n) / (f - n)
    uint view_normal_depth_texture_index;
    uint light_type;

    uint noise_texture_index;
    uint frame_index;
    float tanh_fov;
    float aspect_ratio;
};

vec2 sample_noise_texture(ivec2 id) {
    // return sample_texel(noise_texture_index, (id + frame_index) & 127, 0).rg;
    vec2 noise = sample_texel(noise_texture_index, id & 127, 0).rg;
    return fract(noise + 0.618033 * frame_index);
}

// Utility function to get a vector perpendicular to an input vector
// (from "Efficient Construction of Perpendicular Vectors Without Branching")
vec3 get_perpendicular_vector(vec3 u) {
    vec3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, vec3(xm, ym, zm));
}

#define PI 3.14159265359
vec3 get_cone_sample(ivec2 id, vec3 light_dir, float cos_theta_max) {
    vec2 rand = sample_noise_texture(id);

    vec3 bitangent = get_perpendicular_vector(light_dir);
    vec3 tangent = cross(bitangent, light_dir);

    float cos_theta = mix(cos_theta_max, 1.0, rand.x);

    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    float phi = rand.y * 2.0 * PI;

    return tangent * (sin_theta * cos(phi)) + bitangent * (sin_theta * sin(phi)) + light_dir * cos_theta;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(id, resolution)))
        return;

    vec2 uv = (id + 0.5) * inv_resolution;
    vec4 view_normal_depth = sample_texture(view_normal_depth_texture_index, u_samplers[SAMPLER_POINT_CLAMP], uv);

    vec3 view_normal = octahedral_decode(view_normal_depth.zw);
    float depth = -unpack_float(view_normal_depth.xy);

    vec3 view_pos = get_view_pos_from_uv_depth(uv, depth, tanh_fov, aspect_ratio);
    // Adding offset in view space, not a good idea
    float visibility = 1.0f;
    float tmin = max(1.0, view_pos.z) * 0.05;
    vec3 world_pos = view_pos_to_world_pos(view_pos + tmin * view_normal, inv_V);

    uint ray_flags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT | gl_RayFlagsCullBackFacingTrianglesEXT;

    vec3 ray_dir;
    float range = 1000.0f;
    if (light_type == LIGHT_TYPE_DIRECTIONAL) {
        ray_dir = direction_or_position;
    } else {
        vec3 dir = direction_or_position - world_pos;
        range = length(dir);
        ray_dir = dir / range;
    }
    rayQueryEXT ray_query;
    rayQueryInitializeEXT(ray_query, tlas, ray_flags, 0xff, world_pos, 0.0f, get_cone_sample(id, ray_dir, cos_angular_radius), range);
    rayQueryProceedEXT(ray_query);

    if (rayQueryGetIntersectionTypeEXT(ray_query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        visibility = 0.0f;
    imageStore(u_visibility_texture, id, vec4(visibility, 0.0f, 0.0f, 0.0f));
}