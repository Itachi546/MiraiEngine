#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "../utils/per-frame-data.glsl"
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "ddgi.glsl"

layout(set = 0, binding = 1) uniform PerFrameDataBindings {
    PerFrameData per_frame_data;
};

layout(location = 0) rayPayloadInEXT DDGIRayPayload p_payload;

void main() {
    vec3 rd = gl_WorldRayDirectionEXT;
    rd.y = -rd.y;
    p_payload.L = sample_texture_cube(per_frame_data.skybox_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], rd).rgb;
}