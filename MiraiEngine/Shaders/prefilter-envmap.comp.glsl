#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform samplerCube u_cubemap;
layout(set = 0, binding = 1, rgba16f) uniform imageCube u_irradiance_map;

#include "utils/cubemap.glsl"

layout(push_constant) uniform PushConstants {
    vec2 prefilter_map_dims;
    vec2 cubemap_dims;
};

void main() {
    ivec3 uv = ivec3(gl_GlobalInvocationID.xyz);
    vec3 direction = normalize(uv_to_xyz(uv, prefilter_map_dims));

    vec3 irradiance = direction * 0.5 + 0.5;
    imageStore(u_irradiance_map, uv, vec4(irradiance, 1.0f));
}