#ifndef PBR_GLSL
#define PBR_GLSL

#define PI 3.14159265359

float D_GGX(float ndoth, float roughness) {
    float a = roughness * roughness;
    float a2 = roughness * roughness;
    float denom = ndoth * ndoth * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float G_SGGX(float ndotv, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float denom = ndotv * (1.0 - k) + k;
    return ndotv / denom;
}

float G_Smith(float ndotv, float ndotl, float roughness) {
    return G_SGGX(ndotl, roughness) * G_SGGX(ndotv, roughness);
}

vec3 F_Schlick(float hdotv, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - hdotv, 0.0, 1.0), 5.0);
}


#endif