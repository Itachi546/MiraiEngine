#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform samplerCube u_cubemap;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_prefilter_map;

#include "utils/cubemap.glsl"

layout(push_constant) uniform PushConstants {
    vec2 prefilter_map_dims;
    vec2 cubemap_dims_and_roughness;
};

#define PI 3.14159265359

float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

// https://www.tobias-franke.eu/log/2014/03/30/notes_on_importance_sampling.html
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness * roughness;
    float phi      = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    vec3 up = abs(N.z) < 0.99 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);

    vec3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    vec3 N = normalize(uv_to_xyz(uv, prefilter_map_dims));
    vec3 R = N;
    vec3 V = R;

    uint SAMPLE_COUNT = 4096u;
    float weight = 0.0;
    vec3 Lo = vec3(0.0);
    float roughness = cubemap_dims_and_roughness.y;
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndotl = max(dot(N, L), 0.0);
        if (ndotl > 0.0) {
            Lo += texture(u_cubemap, L).rgb * ndotl;
            weight += ndotl;
        }
    }

    Lo /= weight;
    imageStore(u_prefilter_map, uv, vec4(Lo, 1.0f));
}