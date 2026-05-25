#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform textureCube u_cubemap;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_prefilter_map;

#include "../utils/cubemap.glsl"
#include "../pbr/pbr.glsl"
#include "../utils/bindless-sampler.glsl"

layout(push_constant) uniform PushConstants {
    vec2 prefilter_map_dims;

    float cubemap_dims;
    float roughness;
};

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    if (any(greaterThanEqual(uv.xy, ivec2(prefilter_map_dims))))
        return;

    vec3 N = normalize(uv_to_xyz(uv, prefilter_map_dims));
    vec3 R = N;
    vec3 V = R;

    uint SAMPLE_COUNT = 4096;
    float weight = 0.0;
    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndotl = max(dot(N, L), 0.0);
        if (ndotl > 0.0) {
            float ndoth = max(dot(N, H), 0.0);
            float D = D_GGX(ndoth, roughness);

            float hdotv = max(dot(H, V), 0.0);
            float pdf = (D * ndoth) / (4.0 * hdotv + 0.0001);

            float sa_texel = (4.0 * PI) / (6.0 * cubemap_dims * cubemap_dims);
            float sa_sample = 1.0 / (float(SAMPLE_COUNT) * pdf + 0.0001);
            float mip_level = roughness == 0.0 ? 0.0 : 0.5 * log2(sa_sample / sa_texel);

            Lo += textureLod(samplerCube(u_cubemap, u_samplers[SAMPLER_LINEAR_CLAMP]), L, mip_level).rgb * ndotl;
            weight += ndotl;
        }
    }

    Lo /= weight;
    imageStore(u_prefilter_map, uv, vec4(Lo, 1.0f));
}