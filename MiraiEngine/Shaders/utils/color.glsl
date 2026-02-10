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

vec3 linear_to_srgb(vec3 col) {
    bvec3 cutoff = lessThan(col, vec3(0.0031308));
    vec3 higher = vec3(1.055) * pow(col, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower = col * vec3(12.92);
    return mix(higher, lower, cutoff);
}

vec3 srgb_to_linear(vec3 srgbIn) {
#define MANUAL_SRGB 1
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
    vec3 linOut = pow(srgbIn.xyz, vec3(2.2));
#else  // SRGB_FAST_APPROXIMATION
    vec3 bLess = step(vec3(0.04045), srgbIn.xyz);
    vec3 linOut = mix(srgbIn.xyz / vec3(12.92), pow((srgbIn.xyz + vec3(0.055)) / vec3(1.055), vec3(2.4)), bLess);
#endif // SRGB_FAST_APPROXIMATION
    return linOut;
#else  // MANUAL_SRGB
    return srgbIn;
#endif // MANUAL_SRGB
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

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
vec3 Uncharted2Tonemap(vec3 color) {
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

#endif