#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "ground-truth.glsl"
layout(location = 0) rayPayloadInEXT RayPayload p_payload;

#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"

void main() {
    vec3 ray_dir = normalize(gl_WorldRayDirectionEXT);
    ray_dir.y = -ray_dir.y;
    p_payload.L = sample_texture_cube(skybox_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], ray_dir).rgb;
}