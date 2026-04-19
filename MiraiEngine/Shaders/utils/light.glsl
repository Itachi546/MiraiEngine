#ifndef LIGHT_GLSL
#define LIGHT_GLSL

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
struct Light {
    vec3 position_or_direction;
    uint light_type;

    vec3 color;
    float radius;

    float intensity;
    bool cast_shadow;
    float _padding[2];
};

#endif