#version 450
layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) uniform texture2D u_input_texture;
layout(set = 0, binding = 1) uniform writeonly image2D u_output_texture;

layout(push_constant) uniform PushConstants {
    vec2 resolution;
    float enable_gamma_correction;
    float _padding;
};

#include "../utils/color.glsl"
#include "../utils/bindless-sampler.glsl"

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(id, resolution)))
        return;

    vec2 inv_resolution = 1.0f / resolution;
    vec2 uv = vec2(id) / resolution;

    vec4 col = texture(sampler2D(u_input_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), uv);

    // @TODO Temp
    if (enable_gamma_correction > 0.5f) {
        col.rgb = linear_to_srgb(ACESFilm(col.rgb));
    }

    imageStore(u_output_texture, id, vec4(col.rgb, 1.0f));
}