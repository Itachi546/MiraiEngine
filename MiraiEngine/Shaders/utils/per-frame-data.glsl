#ifndef PER_FRAME_DATA_GLSL
#define PER_FRAME_DATA_GLSL

struct PerFrameData {
    mat4 P;
    mat4 V;
    mat4 VP;
    mat4 invVP;

    vec3 camera_position;
    float elapsed_time;

    vec3 light_direction;
    float cast_shadow;


    vec3 light_color;
    float light_intensity;

    float width;
    float height;
    uint irradiance_map;
    uint prefilter_map;

    uint brdf_texture_map;
    uint padding[3];
};

#endif