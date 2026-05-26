#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_samplerless_texture_functions : require

#include "../utils/raycast.glsl"
#include "ground-truth.glsl"
#include "../utils/bindless-texture.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1, rgba8) uniform image2D output_image;

layout(location = 0) rayPayloadEXT RayPayload p_payload;

void main() {
    const ivec2 launch_id = ivec2(gl_LaunchIDEXT.xy);
    vec2 uv = vec2(launch_id + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    uv = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    vec3 origin = camera_position.xyz;
    vec3 direction = generate_camera_ray(uv, invP, invV);

    p_payload.rng = rng_init(launch_id, frame_count);
    p_payload.L = vec3(0.0f);
    p_payload.depth = 0;
    p_payload.T = vec3(1.0f);

    traceRayEXT(tlas, gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT, 0xff, 0, 0, 0, origin, 0.01, direction, 1000.0, 0);

    if (frame_count == 0) {
        imageStore(output_image, launch_id, vec4(p_payload.L, 1.0f));
    } else {
        vec3 current_col = p_payload.L;
        vec3 prev_color = sample_texel(input_texture_index, launch_id, 0).rgb;
        vec3 accumulated_color = prev_color + (current_col - prev_color) / float(frame_count);
        imageStore(output_image, launch_id, vec4(accumulated_color, 1.0f));
    }
}