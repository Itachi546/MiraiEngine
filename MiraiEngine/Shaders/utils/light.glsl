#ifndef LIGHT_GLSL
#define LIGHT_GLSL

#define LIGHT_TYPE_DIRECTIONAL 0
#define LIGHT_TYPE_POINT 1
#define LIGHT_TYPE_SPOT 2

struct Light {
    vec3 position;
    // 0-2 (LightType)
    // 3 (CastShadow)
    uint flag;

    // SpotDirection or direction for directional light
    vec3 direction;
    float intensity;

    uint color;
    float radius;

    float inner_angle;
    float outer_angle;
};

uint get_light_type(uint light_flag) {
    return light_flag & 7;
}

bool cast_shadow(uint light_flag) {
    return (light_flag & 8) == 8;
}

#define MAX_LIGHT_PER_TILE 256
#define LIGHT_TILE_SIZE 16

uint get_tile_address_opaque(uvec2 tile_id, uvec2 tile_count) {
    return (tile_id.y * tile_count.x + tile_id.x) * (MAX_LIGHT_PER_TILE + 1);
}

uint get_tile_address_transparent(uvec2 tile_id, uvec2 tile_count) {
    return (tile_count.x * tile_count.y + tile_id.y * tile_count.x + tile_id.x) * (MAX_LIGHT_PER_TILE + 1);
}

#endif