#version 460

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#include "ddgi.glsl"

layout(location = 0) rayPayloadInEXT DDGIRayPayload p_payload;

void main() {
    p_payload.L = vec3(1.0f);
    p_payload.hit_distance = gl_HitTEXT;
}