#version 460

#define ENABLE_SAMPLER 1

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#include "utils/transform.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) writeonly uniform image2D u_output_texture;
layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

#if ENABLE_SAMPLER
#include "utils/bindless-sampler.glsl"
#endif

#include "utils/raycast.glsl"

layout(push_constant) uniform ShaderPushConstants {
    mat4 invP;
    mat4 invV;
    vec4 camera_position;
    uint width;
    uint height;
    float znear;
    float zfar;
};

void main() {
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x > width || iuv.y > height)
        return;
    vec2 uv = vec2(iuv + 0.5) / vec2(width, height);

    uv.x = uv.x * 2.0f - 1.0f;
    uv.y = 1.0f - uv.y * 2.0f;

    vec3 r0 = camera_position.xyz;
    vec3 rd = generate_camera_ray(uv, invP, invV);

    uint ray_flags = gl_RayFlagsOpaqueEXT | gl_RayFlagsCullBackFacingTrianglesEXT;

    float depth = 0.0f;
    rayQueryEXT ray_query;
    rayQueryInitializeEXT(ray_query, tlas, ray_flags, 0xff, r0, 0.0, rd, 100.0);
    rayQueryProceedEXT(ray_query);
    if (rayQueryGetIntersectionTypeEXT(ray_query, true) != gl_RayQueryCommittedIntersectionNoneEXT) {
        depth = rayQueryGetIntersectionTEXT(ray_query, true);
    }

    vec3 color = vec3(depth * 0.03);
    imageStore(u_output_texture, iuv, vec4(color, 1.0));
}