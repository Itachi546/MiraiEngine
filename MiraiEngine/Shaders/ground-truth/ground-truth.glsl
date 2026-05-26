#ifndef GROUND_TRUTH_GLSL
#define GROUND_TRUTH_GLSL

layout(push_constant) uniform PushConstant {
    mat4 invP;
    mat4 invV;

    vec3 camera_position;
    uint skybox_texture_index;
};

#endif