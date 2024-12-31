#ifndef SHADOW_GLSL
#define SHADOW_GLSL

#define NUM_DIRLIGHT_CASCADE 4

struct CascadeInfo {
    mat4 VP[NUM_DIRLIGHT_CASCADE];
    vec4 split_distances;
    vec4 dims;
};

uint CASCADE_COLORS[5] = uint[5](
    0xff0000ff,
    0x00ff00ff,
    0x0000ffff,
    0xffff00ff,
    0xff00ffff);

#endif