#ifndef PBR_GLSL
#define PBR_GLSL

#include "../utils/math.glsl"

#define EPSILON 0.0001f

float D_GGX(float ndoth, float alpha) {
    float a2 = alpha * alpha;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float G1_Schlick_GGX(float roughness, float NdotV) {
    float k = ((roughness + 1) * (roughness + 1)) / 8.0;
    return NdotV / max(EPSILON, (NdotV * (1 - k) + k));
}

float G_Schlick_GGX(float NdotL, float NdotV, float roughness) {
    return G1_Schlick_GGX(roughness, NdotL) * G1_Schlick_GGX(roughness, NdotV);
}

vec3 F_Schlick(vec3 F0, float VdotH) {
    return F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

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
vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
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

#endif