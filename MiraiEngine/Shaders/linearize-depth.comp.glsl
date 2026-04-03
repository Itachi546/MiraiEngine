#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "utils/transform.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform texture2D u_depth_texture;
layout(set = 0, binding = 1) writeonly uniform image2D u_output_texture;

layout(push_constant) uniform ShaderPushConstants {
    uint width;
    uint height;
    float znear;
    float zfar;
};

void main() {
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x > width  || iuv.y > height)
       return;

    vec2 uv = vec2(iuv + 0.5) / vec2(width, height);
    float depth = texelFetch(u_depth_texture, iuv, 0).r;// texture(sampler2D(u_depth_texture, u_samplers), iuv).r;
    float linear_depth = linearize_depth(depth, znear, zfar) * 0.05;
    imageStore(u_output_texture, iuv, vec4(linear_depth, linear_depth, linear_depth, 1.0));
}