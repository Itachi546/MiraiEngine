#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_KHR_shader_subgroup_basic : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "../utils/frustum.glsl"
#include "../utils/light.glsl"
#include "../utils/transform.glsl"

layout(local_size_x = LIGHT_TILE_SIZE, local_size_y = LIGHT_TILE_SIZE, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_depth_texture;

layout(std430, set = 0, binding = 1) writeonly buffer LightList {
    uint light_lists[];
};

layout(std430, set = 0, binding = 2) readonly buffer LightBuffer {
    Light lights[];
};

layout(push_constant) uniform PushConstant {
    mat4 invP;
    mat4 V;
    uint light_count;
    uint tile_count_x;
    uint tile_count_y;
    uint depth_texture_width;

    uint depth_texture_height;
    uint tile_size;
    uint _padding[2];
};

shared uint s_tile_opaque_list[MAX_LIGHT_PER_TILE];
shared uint s_tile_transparent_list[MAX_LIGHT_PER_TILE];

shared uint s_tile_min_depth;
shared uint s_tile_max_depth;
shared uint s_opaque_light_count;
shared uint s_transparent_light_count;
shared TileFrustum s_frustum;

void append_light_opaque(uint light_index) {
    uint index = atomicAdd(s_opaque_light_count, 1);
    if (index < MAX_LIGHT_PER_TILE)
        s_tile_opaque_list[index] = light_index;
}

void append_light_transparent(uint light_index) {
    uint index = atomicAdd(s_transparent_light_count, 1);
    if (index < MAX_LIGHT_PER_TILE)
        s_tile_transparent_list[index] = light_index;
}

vec4 compute_plane(vec3 p0, vec3 p1, vec3 p2) {
    vec3 e0 = p1 - p0;
    vec3 e1 = p2 - p0;
    vec4 plane;
    plane.xyz = normalize(cross(e0, e1));
    plane.w = -dot(plane.xyz, p0);
    return plane;
}

vec2 to_ndc(vec2 coord, vec2 dim_rcp) {
    return vec2((coord.x * dim_rcp.x) * 2.0 - 1.0, 1.0 - (coord.y * dim_rcp.y) * 2.0);
}

void calculate_tile_frustum(ivec2 id, out TileFrustum frustum) {
    vec2 dim_rcp = 1.0f / vec2(depth_texture_width, depth_texture_height);

    ivec2 origin = id * ivec2(tile_size);
    vec2 tl_ndc = to_ndc(origin, dim_rcp);
    vec2 tr_ndc = to_ndc(origin + ivec2(tile_size, 0), dim_rcp);
    vec2 bl_ndc = to_ndc(origin + ivec2(0, tile_size), dim_rcp);
    vec2 br_ndc = to_ndc(origin + tile_size, dim_rcp);

    // Compute view space position of frustum far plane
    vec3 tl = clip_pos_to_view_pos(vec3(tl_ndc, 1), invP);
    vec3 tr = clip_pos_to_view_pos(vec3(tr_ndc, 1), invP);
    vec3 bl = clip_pos_to_view_pos(vec3(bl_ndc, 1), invP);
    vec3 br = clip_pos_to_view_pos(vec3(br_ndc, 1), invP);

    // Camera position is at origin in view space
    vec3 p0 = vec3(0.0);

    // Left
    frustum.planes[0] = compute_plane(p0, tl, bl);

    // Right
    frustum.planes[1] = compute_plane(p0, br, tr);

    // Top
    frustum.planes[2] = compute_plane(p0, tl, tr);

    // Bottom
    frustum.planes[3] = compute_plane(p0, br, bl);
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    ivec2 group_id = ivec2(gl_WorkGroupID.xy);
    uint groupIndex = gl_LocalInvocationIndex;

    // Initialize the bucket
    for (uint i = groupIndex; i < MAX_LIGHT_PER_TILE; i += LIGHT_TILE_SIZE * LIGHT_TILE_SIZE) {
        s_tile_opaque_list[i] = 0;
        s_tile_transparent_list[i] = 0;
    }

    // Initialize per group variables
    if (groupIndex == 0) {
        // Using float max to be on safe side
        s_tile_min_depth = 0xFFFFFFFF;
        s_tile_max_depth = 0;
        s_opaque_light_count = 0;
        s_transparent_light_count = 0;
        calculate_tile_frustum(group_id, s_frustum);
    }

    memoryBarrierShared();
    barrier();

    ivec2 depth_uv = clamp(id, ivec2(0), ivec2(depth_texture_width - 1, depth_texture_height - 1));
    float depth = texelFetch(u_depth_texture, depth_uv, 0).r;

    // Wave min and max depth
    float wave_min_depth = subgroupMin(depth);
    float wave_max_depth = subgroupMax(depth);

    // Group min/max depth reduction
    if (subgroupElect()) {
        atomicMin(s_tile_min_depth, floatBitsToUint(wave_min_depth));
        atomicMax(s_tile_max_depth, floatBitsToUint(wave_max_depth));
    }

    memoryBarrierShared();
    barrier();

    float fmin_depth = uintBitsToFloat(s_tile_min_depth);
    float fmax_depth = uintBitsToFloat(s_tile_max_depth);

    // Calculate min/max depth in view space for frustum culling
    float min_depth_vs = clip_pos_to_view_pos(vec3(0.0, 0.0, fmin_depth), invP).z;
    float max_depth_vs = clip_pos_to_view_pos(vec3(0.0, 0.0, fmax_depth), invP).z;
    float near_clip_vs = clip_pos_to_view_pos(vec3(0.0, 0.0, 0.0), invP).z;
    // The near and far plane is simple in view space
    vec4 min_plane = vec4(0.0f, 0.0f, -1.0f, min_depth_vs);

    for (uint i = groupIndex; i < light_count; i += LIGHT_TILE_SIZE * LIGHT_TILE_SIZE) {
        Light light = lights[i];
        uint light_type = get_light_type(light.flag);
        if (light_type == LIGHT_TYPE_DIRECTIONAL) {
            append_light_opaque(i);
            append_light_transparent(i);
        } else if (light_type == LIGHT_TYPE_POINT) {
            vec4 light_position_vs = V * vec4(light.position, 1.0f);
            vec4 sphere = vec4(light_position_vs.xyz, light.radius_or_height);
            if (sphere_inside_frustum(s_frustum, sphere, near_clip_vs, max_depth_vs)) {
                append_light_transparent(i);
                if (sphere_inside_plane(sphere, min_plane))
                    append_light_opaque(i);
            }
        } else if (light_type == LIGHT_TYPE_SPOT) {
            vec4 light_position_vs = V * vec4(light.position, 1.0f);
            vec3 light_direction_vs = normalize(vec3(V * vec4(light.direction, 0.0f)));
            float height = light.radius_or_height;
            float radius = height * tan(light.outer_angle);
            if (cone_inside_frustum(s_frustum, light_position_vs.xyz, light_direction_vs.xyz, height, radius, near_clip_vs, max_depth_vs)) {
                append_light_transparent(i);
                vec3 base = light_position_vs.xyz + light_direction_vs.xyz * height;
                if (cone_inside_plane(min_plane, light_position_vs.xyz, light_direction_vs.xyz, base, radius))
                    append_light_opaque(i);
            }
        }
    }

    memoryBarrierShared();
    barrier();

    // The additional +1 offset is the first uint32_t used to track the number of light per tile
    uint opaque_bucket_address = get_tile_address_opaque(group_id, ivec2(tile_count_x, tile_count_y));
    uint transparent_bucket_address = get_tile_address_transparent(group_id, ivec2(tile_count_x, tile_count_y));

    if (groupIndex == 0) {
        light_lists[opaque_bucket_address] = min(s_opaque_light_count, MAX_LIGHT_PER_TILE);
        light_lists[transparent_bucket_address] = min(s_transparent_light_count, MAX_LIGHT_PER_TILE);
    }

    uint opaque_light_count = min(s_opaque_light_count, MAX_LIGHT_PER_TILE);
    for (uint i = groupIndex; i < opaque_light_count; i += LIGHT_TILE_SIZE * LIGHT_TILE_SIZE) {
        light_lists[opaque_bucket_address + i + 1] = s_tile_opaque_list[i];
    }

    uint transparent_light_count = min(s_transparent_light_count, MAX_LIGHT_PER_TILE);
    for (uint i = groupIndex; i < transparent_light_count; i += LIGHT_TILE_SIZE * LIGHT_TILE_SIZE) {
        light_lists[transparent_bucket_address + i + 1] = s_tile_transparent_list[i];
    }
}