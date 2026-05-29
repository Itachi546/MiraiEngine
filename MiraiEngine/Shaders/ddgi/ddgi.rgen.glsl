#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../utils/per-frame-data.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1) uniform PerFrameDataBindings {
    PerFrameData per_frame_data;
};
layout(set = 0, binding = 2, rgba16f) uniform image2D u_radiance_texture;
layout(set = 0, binding = 3, rg16f) uniform image2D u_distance_texture;

#include "ddgi.glsl"
layout(push_constant) uniform PushConstants {
    DDGISettings ddgi_settings;
};

layout(location = 0) rayPayloadEXT DDGIRayPayload p_payload;

void main() {
    const ivec2 pixel_coord = ivec2(gl_LaunchIDEXT.xy);
    const int probe_index = pixel_coord.y;
    const int ray_id = pixel_coord.x;

    vec3 ray_origin = probe_location(probe_index, ddgi_settings);
    vec3 ray_direction = normalize(mat3(ddgi_settings.random_orientation) * spherical_fibonacci(pixel_coord.x, ddgi_settings.ray_per_probe));

    uint ray_flags = gl_RayFlagsOpaqueEXT;
    uint cull_mask = 0xff;
    float tmin = 0.001f;
    float tmax = 10000.0f;

    p_payload.L = vec3(0.0f);
    p_payload.rng = rng_init(pixel_coord, ddgi_settings.frame_count);
    p_payload.T = vec3(1.0f);
    p_payload.hit_distance = tmax;

    traceRayEXT(tlas, ray_flags, cull_mask,
                0, 0, 0,
                ray_origin, tmin,
                ray_direction, tmax,
                0);

    imageStore(u_radiance_texture, pixel_coord, vec4(p_payload.L, 1.0f));
    imageStore(u_distance_texture, pixel_coord, vec4(p_payload.hit_distance / 1000.0f, 0.0f, 0.0f, 1.0f));
}