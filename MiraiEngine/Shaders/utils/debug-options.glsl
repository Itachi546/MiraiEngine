#ifndef DEBUG_OPTIONS_GLSL
#define DEBUG_OPTIONS_GLSL

#define DEBUG_ALBEDO 0
#define DEBUG_NORMAL 1
#define DEBUG_METALLIC 2
#define DEBUG_ROUGHNESS 3
#define DEBUG_AO 4
#define DEBUG_SHADOW 5
#define DEBUG_CSM_SPLIT 6
#define DEBUG_LIGHT_TILE 7
#define DEBUG_VELOCITY 8

vec3 get_tile_heatmap(uint light_count, uint max_heat) {
    const vec3 map_tex[] = {
        vec3(0.0, 0.0, 0.0),
        vec3(0.0, 0.0, 1.0),
        vec3(0.0, 1.0, 1.0),
        vec3(0.0, 1.0, 0.0),
        vec3(1.0, 1.0, 1.0),
        vec3(1.0, 0.0, 0.0),
    };

    uint max_tex_len = 5;
    float l = clamp(float(light_count) / float(max_heat), 0.0, 1.0) * max_tex_len;
    vec3 a = map_tex[int(floor(l))];
    vec3 b = map_tex[int(ceil(l))];

#if 1
    return mix(a, b, l - floor(l));
#else
    return map_tex[light_count + 1];
#endif
}

#endif