#version 460

#extension GL_GOOGLE_include_directive : enable
#include "../utils/cubemap.glsl"
#include "../utils/bindless-sampler.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform texture2D u_hdri;
layout(set = 0, binding = 1, rgba16f) writeonly uniform imageCube u_cubemap;

layout(push_constant) uniform PushConstants {
    vec2 cubemap_size;
    vec2 _padding;
};

const vec2 inv_atan = vec2(0.1591, 0.3183);
vec2 spherical_coord(vec3 p) {
    vec2 uv = vec2(atan(p.z, p.x), asin(p.y));
    uv *= inv_atan;
    uv += 0.5f;
    return uv;
}

void main() {
    ivec3 cube_coord = ivec3(gl_GlobalInvocationID.xyz);
    vec3 p = normalize(uv_to_xyz(cube_coord, cubemap_size));
    vec3 color = texture(sampler2D(u_hdri, u_samplers[SAMPLER_LINEAR_CLAMP]), spherical_coord(p)).rgb;
    imageStore(u_cubemap, cube_coord, vec4(color, 1.0f));
}