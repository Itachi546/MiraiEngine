#ifndef TRANSFORMATION_GLSL
#define TRANSFORMATION_GLSL

vec3 clip_pos_to_world_pos(vec3 clip_pos, mat4 invVP) {
    vec4 world_pos = invVP * vec4(clip_pos, 1.0f);
    return world_pos.xyz / world_pos.w;
}

#endif