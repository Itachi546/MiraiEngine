#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

layout(binding = 0) uniform accelerationStructureEXT tlas;
layout(binding = 1, rgba8) uniform image2D output_image;

layout(location = 0) rayPayloadEXT vec3 hit_color;

void main() {
    const ivec2 launch_id = ivec2(gl_LaunchIDEXT.xy);
    vec2 uv = vec2(launch_id + 0.5) / vec2(gl_LaunchSizeEXT.xy);

    vec3 origin = vec3(0.0f, 0.0f, -2.0f);
    vec3 direction = normalize(vec3(uv * 2.0 - 1.0, 1.0));

    traceRayEXT(tlas, gl_RayFlagsOpaqueEXT, 0xff, 0, 0, 0, origin, 0.01, direction, 1000.0, 0);
    imageStore(output_image, launch_id, vec4(hit_color, 1.0));
}