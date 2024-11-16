#ifndef COLOR_GLSL
#define COLOR_GLSL

const float INV_255 = 1.0f / 255.0f;

vec4 u32_to_rgba(uint color)
{
    return vec4((color >> 24) & 0xff,
                (color >> 16) & 0xff,
                (color >> 8) & 0xff,
                color & 0xff) *
           INV_255;
}

vec3 gamma(vec3 col)
{
    return pow(col, vec3(0.4545));
}

vec3 inv_gamma(vec3 col)
{
    return pow(col, vec3(2.2f));
}

float rgb_to_luma(vec3 col)
{
    return dot(col, vec3(0.299, 0.587, 0.114));
}

#endif