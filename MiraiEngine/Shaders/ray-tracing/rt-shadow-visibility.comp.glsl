#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_ray_query : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/light.glsl"
#include "../utils/noise.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, r8) uniform writeonly image2D u_visibility_texture;
layout(binding = 1) uniform accelerationStructureEXT tlas;

layout(push_constant) uniform PushConstantData {
    mat4 inv_VP;

    vec2 resolution;
    vec2 inv_resolution;

    // Only support point and directional light for now
    vec3 direction_or_position;
    float radius;

    uint depth_texture_index;
    uint light_type;
    uint noise_texture_index;
    int frame_index;
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
vec3 get_cone_sample(ivec2 id, vec3 lightDir, float cosThetaMax) {
    vec2 rand = sample_noise_texture(id);

    vec3 bitangent = get_perpendicular_vector(lightDir);
    vec3 tangent = cross(bitangent, lightDir);

    float cosTheta = mix(cosThetaMax, 1.0, rand.x);

    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float phi = rand.y * 2.0 * PI;

    return tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + lightDir * cosTheta;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(id, resolution)))
        return;

    float depth = sample_texel(depth_texture_index, id, 0).r;

    vec2 uv = (id + 0.5) * inv_resolution;

    vec3 current_ndc_pos = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
    vec3 world_pos = ndc_pos_to_world_pos(current_ndc_pos, inv_VP);

    float visibility = 1.0f;
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
    float tmin = max(1.0f, length(world_pos)) * 0.05f;
    rayQueryInitializeEXT(ray_query, tlas, ray_flags, 0xff, world_pos, tmin, get_cone_sample(id, ray_dir, 0.995), range);
    rayQueryProceedEXT(ray_query);
    if (rayQueryGetIntersectionTypeEXT(ray_query, true) != gl_RayQueryCommittedIntersectionNoneEXT)
        visibility = 0.0f;

    imageStore(u_visibility_texture, id, vec4(visibility, 0.0f, 0.0f, 0.0f));
}