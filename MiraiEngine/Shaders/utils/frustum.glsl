#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

struct TileFrustum {
    // Left, Right, Top, Bottom
    vec4 planes[4];
};

#endif