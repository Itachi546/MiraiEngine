#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0, rg16f) uniform image2D u_brdf_texture;

#include "utils/cubemap.glsl"
#include "utils/pbr.glsl"

layout(push_constant) uniform PushConstants {
    vec2 inv_brdf_texture_size;
};

vec2 IntegrateBRDF(float ndotv, float roughness) {
    vec3 V;
    V.x = sqrt(1.0 - ndotv * ndotv);
    V.y = 0.0f;
    V.z = ndotv;

    vec2 brdf = vec2(0.0f);
    vec3 N = vec3(0.0f, 0.0f, 1.0f);

    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float ndotl = max(L.z, 0.0);
        float ndoth = max(H.z, 0.0);
        float vdoth = max(dot(V, H), 0.0);

        if (ndotl > 0.0) {
            float G = G_Smith(ndotv, ndotl, roughness);
            float G_Vis = (G * vdoth) / (ndoth * ndotv);
            float Fc = pow(1.0 - vdoth, 5.0);

            brdf.x += (1.0 - Fc) * G_Vis;
            brdf.y += Fc * G_Vis;
        }
    }

    float inv_sample_count = 1.0f / float(SAMPLE_COUNT);
    return brdf * inv_sample_count;
}

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    vec2 tex_coord = (vec2(uv.xy) + 0.5f) * inv_brdf_texture_size;
    vec2 brdf = IntegrateBRDF(tex_coord.x, 1.0 - tex_coord.y);
    imageStore(u_brdf_texture, uv.xy, vec4(brdf, 0.0f, 0.0f));
}