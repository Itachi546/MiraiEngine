#ifndef PBR_GLSL
#define PBR_GLSL

#define PI 3.14159265359
const float MAX_REFLECTION_LOD = 6.0;

float D_GGX(float ndoth, float roughness) {
    float a = roughness * roughness;
    float a2 = roughness * roughness;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float G_SGGX(float ndotv, float roughness) {
    /*
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denom = ndotv * (1.0 - k) + k;
    return ndotv / max(denom, 0.0001f);
    */
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom = ndotv;
    float denom = ndotv * (1.0 - k) + k;

    return nom / max(denom, 0.00001f);
}

float G_Smith(float ndotv, float ndotl, float roughness) {
    return G_SGGX(ndotl, roughness) * G_SGGX(ndotv, roughness);
}

vec3 F_Schlick(float hdotv, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - hdotv, 0.0, 1.0), 5.0);
}

vec3 F_SchlickRoughness(float hdotv, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - hdotv, 0.0, 1.0), 5.0);
}

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