#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/color.glsl"

layout(set = 0, binding = 0, r11f_g11f_b10f) uniform image2D u_output_texture;

layout(push_constant) uniform PushConstants {
    uint input_texture_index;
    uint mip_level;
    uint width;
    uint height;

    float radius;
    float _padding[3];
};

vec3 get_sample(vec2 uv) {
    return sample_texture_lod(input_texture_index, u_samplers[SAMPLER_LINEAR_CLAMP], uv, mip_level).rgb;
}

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    vec2 resolution = vec2(width, height);
    if (any(greaterThanEqual(id, resolution)))
        return;

    // Prev mip level texel size
    vec2 inv_res = 1.0f / resolution;
    vec2 uv = (id + 0.5) * inv_res;

    // We are reading from next mip, so texel size is two time the current
    vec2 texel_size = inv_res * 2.0f * radius;

    vec3 a = get_sample(uv + vec2(-texel_size.x, -texel_size.y));
    vec3 b = get_sample(uv + vec2(0, -texel_size.y));
    vec3 c = get_sample(uv + vec2(texel_size.x, -texel_size.y));

    vec3 d = get_sample(uv + vec2(-texel_size.x, 0));
    vec3 e = get_sample(uv + vec2(0, 0));
    vec3 f = get_sample(uv + vec2(texel_size.x, 0));

    vec3 g = get_sample(uv + vec2(-texel_size.x, texel_size.y));
    vec3 h = get_sample(uv + vec2(0, texel_size.y));
    vec3 i = get_sample(uv + vec2(texel_size.x, texel_size.y));

    vec3 col = (b + d + f + h) * 2.0f;
    col += (a + c + g + i);
    col += e * 4.0f;
    col *= 0.0625f;

    vec3 current_color = imageLoad(u_output_texture, id).rgb;
    imageStore(u_output_texture, id, vec4(col + current_color, 1.0f));
}
