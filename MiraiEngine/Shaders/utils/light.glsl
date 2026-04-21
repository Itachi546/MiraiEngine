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
    float cast_shadow;
    float _padding[2];
};

#define MAX_LIGHT_PER_TILE 256
#define LIGHT_TILE_SIZE 16

uint get_tile_address_opaque(uvec2 tile_id, uvec2 tile_count) {
    return (tile_id.y * tile_count.x + tile_id.x) * (MAX_LIGHT_PER_TILE + 1);
}

uint get_tile_address_transparent(uvec2 tile_id, uvec2 tile_count) {
    return (tile_count.x * tile_count.y + tile_id.y * tile_count.x + tile_id.x) * (MAX_LIGHT_PER_TILE + 1);
}

#endif