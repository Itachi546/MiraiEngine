#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_ray_query : enable

#include "utils/transform.glsl"
#include "utils/noise.glsl"

layout(set = 0, binding = 0, r16f) uniform image2D u_rt_shadow_texture;
layout(set = 0, binding = 1) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 2) uniform accelerationStructureEXT tlas;

layout(push_constant) uniform RTShadowPushConstants {
    mat4 invVP;

    vec3 light_direction;
    float width;
    float height;
};

const float SUN_JITTER = 0.01f;

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    if (id.x >= width || id.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(id.xy) * inv_res;

    float depth = texture(u_depth_texture, uv).r;
    vec3 clip_pos = vec3(uv * 2.0 - 1.0, depth);
    vec3 world_pos = clip_pos_to_world_pos(clip_pos, invVP);

    float dir0 = gradientNoise(vec2(id.xy));
    float dir1 = gradientNoise(vec2(id.yx));

    vec3 dir = light_direction;
    dir.x += (dir0 * 2 - 1) * SUN_JITTER;
    dir.z += (dir1 * 2 - 1) * SUN_JITTER;
    dir = normalize(dir);

    float shadow_factor = 1.0f;
    rayQueryEXT ray_query;
    rayQueryInitializeEXT(ray_query, tlas, gl_RayFlagsTerminateOnFirstHitEXT, 0xff, world_pos, 0.01, dir, 1000.0);
    rayQueryProceedEXT(ray_query);
    if (rayQueryGetIntersectionTypeEXT(ray_query, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
        shadow_factor = 0.1f;

    imageStore(u_rt_shadow_texture, id.xy, vec4(shadow_factor));
}