#version 460
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#extension GL_NV_compute_shader_derivatives : enable
#include "utils/transform.glsl"

layout(set = 0, binding = 0, r16f) uniform image2D u_rt_shadow_texture;
layout(set = 0, binding = 1) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 2) uniform sampler2D u_normal_texture;

layout(push_constant) uniform HBAOPushConstants {
    mat4 inv_projection_matrix;

    float width;
    float height;
};

void main() {
    ivec3 id = ivec3(gl_GlobalInvocationID.xyz);
    if (id.x >= width || id.y >= height)
        return;

    vec2 inv_res = 1.0f / vec2(width, height);
    vec2 uv = vec2(id.xy) * inv_res;

    imageStore(u_rt_shadow_texture, id.xy, vec4(1.0f));
}