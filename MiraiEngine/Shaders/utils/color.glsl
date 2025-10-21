#ifndef COLOR_GLSL
#define COLOR_GLSL

const float INV_255 = 1.0f / 255.0f;

vec4 u32_to_rgba(uint color) {
    return vec4((color >> 24) & 0xff,
                (color >> 16) & 0xff,
                (color >> 8) & 0xff,
                color & 0xff) *
           INV_255;
}

vec3 gamma(vec3 col) {
    return pow(col, vec3(0.4545));
}

vec3 inv_gamma(vec3 col) {
    return pow(col, vec3(2.2f));
}

float rgb_to_luma(vec3 col) {
    return dot(col, vec3(0.299, 0.587, 0.114));
}

vec3 ACESFilm(vec3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Filmic(const vec3 hdr) {
    vec3 x = max(vec3(0.0f), hdr - 0.004f);
    return (x * (6.2f * x + 0.5f)) / (x * (6.2f * x + 1.7f) + 0.06f);
}

#endif