#ifndef GROUND_TRUTH_GLSL
#define GROUND_TRUTH_GLSL

#include "../utils/random.glsl"

layout(push_constant) uniform PushConstant {
    mat4 invP;
    mat4 invV;

    vec3 camera_position;
    uint skybox_texture_index;

    uint frame_count;
    uint input_texture_index;
    uint _padding[2];
};

struct RayPayload {
    vec3 L;
    uint depth;

    vec3 T;

    RNG rng;
};

#endif