#version 460

#extension GL_GOOGLE_include_directive : enable

#include "utils/transform.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform sampler2D u_depth_texture;
layout(set = 0, binding = 1) writeonly uniform image2D u_output_texture;

layout(push_constant) uniform ShaderPushConstants {
    uint width;
    uint height;
    float znear;
    float zfar;
};

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    if (uv.x > width - 1 || uv.y > height - 1)
        return;

    float depth = texelFetch(u_depth_texture, uv, 0).r;
    float linear_depth = linearize_depth(depth, znear, zfar) * 0.05;
    imageStore(u_output_texture, uv, vec4(linear_depth));
}