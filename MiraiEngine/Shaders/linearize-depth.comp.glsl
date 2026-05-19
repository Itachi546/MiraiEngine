#version 460

#define ENABLE_SAMPLER 1

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "utils/transform.glsl"
#include "utils/bindless-texture.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) writeonly uniform image2D u_output_texture;

#if ENABLE_SAMPLER
#include "utils/bindless-sampler.glsl"
#endif

layout(push_constant) uniform ShaderPushConstants {
    uint width;
    uint height;
    float znear;
    float zfar;

    uint depth_texture_index;
    uint padding[3];
};

void main() {
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x > width || iuv.y > height)
        return;

    vec2 uv = vec2(iuv + 0.5) / vec2(width, height);
    float depth = sample_texel(depth_texture_index, iuv, 0).r;
    float linear_depth = linearize_depth(depth, znear, zfar);
    imageStore(u_output_texture, iuv, vec4(vec3(-linear_depth * 0.05), 1.0));
}