#version 460

#extension GL_GOOGLE_include_directive : enable

#include "../utils/bindless-sampler.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(set = 0, binding = 0) uniform texture2D u_input_texture;
layout(set = 0, binding = 1, rgba8) writeonly uniform image2D u_output_texture;

layout(push_constant) uniform PushConstants {
    uint width;
    uint height;
    uint padding[2];
};

void main() {
    ivec2 iuv = ivec2(gl_GlobalInvocationID.xy);
    if (iuv.x > width || iuv.y > height)
        return;

    vec2 uv = vec2(iuv + 0.5) / vec2(width, height);
    vec4 val = texture(sampler2D(u_input_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), uv).rgba;
    imageStore(u_output_texture, iuv, vec4(val.xyz, 1.0));
}