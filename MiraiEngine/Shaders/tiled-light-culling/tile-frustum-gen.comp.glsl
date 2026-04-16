#version 460

#extension GL_GOOGLE_include_directive : enable
#include "../utils/transform.glsl"
#include "../utils/frustum.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0) writeonly buffer TileFrustumBuffer {
    TileFrustum frustums[];
};

layout(push_constant) uniform PushConstant {
    mat4 invP;
    vec2 resolution;
    float tile_size;
    float _padding;
};

vec2 to_ndc(vec2 coord, vec2 resolution) {
    return (coord / resolution) * 2.0 - 1.0;
}

vec4 compute_plane(vec3 p0, vec3 p1, vec3 p2) {
    vec3 e0 = p1 - p0;
    vec3 e1 = p2 - p0;
    vec4 plane;
    plane.xyz = normalize(cross(e0, e1));
    plane.w = dot(plane.xyz, p0);
    return plane;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec2 tl_coord = id * tile_size;
    vec2 br_coord = tl_coord + tile_size;

    // Compute ndc coordinate of tile
    vec2 br_ndc = to_ndc(br_coord, resolution);
    vec2 tl_ndc = to_ndc(tl_coord, resolution);

    vec2 bl_ndc = vec2(tl_ndc.x, br_ndc.y);
    vec2 tr_ndc = vec2(br_ndc.x, tl_ndc.y);

    // Compute view space position of frustum far plane
    vec3 bl = clip_pos_to_view_pos(vec3(bl_ndc, 1.0), invP);
    vec3 br = clip_pos_to_view_pos(vec3(br_ndc, 1.0), invP);
    vec3 tl = clip_pos_to_view_pos(vec3(tl_ndc, 1.0), invP);
    vec3 tr = clip_pos_to_view_pos(vec3(tr_ndc, 1.0), invP);

    // Camera position is at origin in view space
    vec3 p0 = vec3(0.0);

    // Tile count along x-axis
    int tile_stride = int((resolution.x + tile_size - 1) / tile_size);
    int buffer_index = id.y * tile_stride + id.x;

    frustums[buffer_index].planes[0] = compute_plane(p0, bl, tl);
    frustums[buffer_index].planes[1] = compute_plane(p0, br, tr);
    frustums[buffer_index].planes[2] = compute_plane(p0, tr, tl);
    frustums[buffer_index].planes[3] = compute_plane(p0, br, bl);
}