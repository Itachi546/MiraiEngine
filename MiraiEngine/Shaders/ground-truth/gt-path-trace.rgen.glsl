#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../utils/raycast.glsl"

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1, rgba8) uniform image2D output_image;

layout(location = 0) rayPayloadEXT vec4 hit_color;

layout(push_constant) uniform PushConstant {
    mat4 invP;
    mat4 invV;
    vec3 camera_position;
    uint skybox_texture_index;
};

void main() {
    const ivec2 launch_id = ivec2(gl_LaunchIDEXT.xy);
    vec2 uv = vec2(launch_id + 0.5) / vec2(gl_LaunchSizeEXT.xy);
    uv = vec2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);

    vec3 origin = camera_position.xyz;
    vec3 direction = generate_camera_ray(uv, invP, invV);

    traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, 0.01, direction, 1000.0, 0);
    imageStore(output_image, launch_id, hit_color);
}