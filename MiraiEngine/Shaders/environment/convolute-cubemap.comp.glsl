#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform textureCube u_cubemap;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_irradiance_map;

#include "../utils/cubemap.glsl"
#include "../utils/bindless-sampler.glsl"

layout(push_constant) uniform PushConstants {
    vec2 irradiance_map_dims;
    vec2 cubemap_dims;
};

// https://www.youtube.com/watch?v=xFsJMUS94Fs&list=PL8vNj3osX2PzZ-cNSqhA8G6C1-Li5-Ck8&index=10&ab_channel=GSNComposer
const int K_MAX_SAMPLES = 180;
const int K_MAX_SAMPLES_IMPORTANCE = 100000;

#define PI 3.14159265359

#define PI2 (PI * 2.0f)
#define PIH (PI * 0.5)

const float dP = PI2 / K_MAX_SAMPLES;
const float dT = PIH / K_MAX_SAMPLES;

vec3 convolute(vec3 direction) {
    // Orthonormal basis
    vec3 normal = normalize(direction);
    vec3 right = normalize(cross(vec3(0.0, 1.0, 0.0), normal));
    vec3 up = cross(normal, right);

    int sample_count = 0;
    vec3 irradiance = vec3(0.0f);
    for (float phi = 0.0f; phi < PI2; phi += dP) {
        for (float theta = 0.0f; theta < PIH; theta += dT) {
            vec3 temp_vec = cos(phi) * right + sin(phi) * up;
            vec3 sample_dir = cos(theta) * normal + sin(theta) * temp_vec;
            irradiance += texture(samplerCube(u_cubemap, u_samplers[SAMPLER_LINEAR_CLAMP]), sample_dir).rgb * cos(theta) * sin(theta);
            sample_count++;
        }
    }

    return (PI * irradiance) / float(sample_count);
}

// hash functions for GPU Rendering.
// http://www.jcgt.org/published/0009/03/02/
vec3 pcg3d(uvec3 v) {

    v = v * 1664525u + 1013904223u;

    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;

    v ^= v >> 16u;

    v.x += v.y * v.z;
    v.y += v.z * v.x;
    v.z += v.x * v.y;

    return vec3(v) * (1.0f / float(0xffffffffu));
}

vec3 convolute_importance_sample(vec3 direction, uvec2 uv) {
    vec3 I = vec3(0.0f);
    // Orthonormal basis
    vec3 normal = normalize(direction);
    vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), normal));
    vec3 bitangent = normalize(cross(normal, tangent));

    uint sample_count = K_MAX_SAMPLES_IMPORTANCE;
    for (uint i = 0; i < sample_count; ++i) {
        vec3 rand = pcg3d(uvec3(uv, i));
        float phi = PI2 * rand.x;
        float theta = asin(sqrt(rand.y));
        float sin_theta = sin(theta);
        vec3 sphere_coord = vec3(cos(phi) * sin_theta, sin(phi) * sin_theta, cos(theta));
        vec3 sample_dir = sphere_coord.x * tangent + sphere_coord.y * bitangent + sphere_coord.z * normal;
        I += texture(samplerCube(u_cubemap, u_samplers[SAMPLER_LINEAR_CLAMP]), normalize(sample_dir)).rgb;
    }
    return I / float(sample_count);
}

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    if (uv.x >= irradiance_map_dims.x || uv.y >= irradiance_map_dims.y)
        return;

    vec3 direction = uv_to_xyz(uv, irradiance_map_dims);
    vec3 irradiance = convolute_importance_sample(direction, uvec2(uv.xy));
    // vec3 irradiance = convolute(direction);
    imageStore(u_irradiance_map, uv, vec4(irradiance, 1.0f));
}