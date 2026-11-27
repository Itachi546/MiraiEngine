#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "../utils/bindless-texture.glsl"
#include "../ddgi/ddgi.glsl"
#include "../utils/transform.glsl"

layout(set = 0, binding = 0, rgba16f) uniform image2D u_irradiance_texture;

layout(push_constant) uniform DDGIIrradiancePushConstant {
    mat4 random_orientation;
    ivec3 probe_counts;
    int ray_per_probe;

    uint ddgi_radiance_texture;
    uint ddgi_depth_texture;
    uint _padding[2];
};

void main() {
    ivec3 probe_grid_coord = ivec3(gl_WorkGroupID.xyz);
    ivec2 t = ivec2(gl_LocalInvocationID.xy);

    vec2 texel_dir = (t + 0.5) / 8.0f;
    vec3 dir = octahedral_decode(texel_dir);

    vec3 sum = vec3(0.0f);
    float wsum = 0.0f;

    int probe_index = grid_coord_to_probe_index(probe_grid_coord, probe_counts);
    for (int ray_index = 0; ray_index < ray_per_probe; ++ray_index) {
        float ray_dist = sample_texel(ddgi_depth_texture, ivec2(ray_index, probe_index), 0).r;
        if (ray_dist <= 0.0f)
            continue;
        vec3 ray_direction = get_probe_ray_direction(ray_index, ray_per_probe, mat3(random_orientation));

        float w = max(dot(ray_direction, dir), 0.0f);

        vec3 radiance = sample_texel(ddgi_radiance_texture, ivec2(ray_index, probe_index), 0).rgb;
        sum += w * radiance;
        wsum += w;
    }

    vec3 irradiance = wsum > 0.0f ? (sum / wsum) : vec3(0.0f);

    ivec2 uv = get_tile_uv(probe_grid_coord, t, probe_counts.x);
    // imageStore(u_irradiance_texture, uv, vec4(probe_grid_coord / (probe_counts - 1.0f), 1.0f));
    imageStore(u_irradiance_texture, uv, vec4(irradiance, 1.0f));
}