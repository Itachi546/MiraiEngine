#version 460

#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_samplerless_texture_functions : enable

#include "../utils/transform.glsl"
#include "../utils/bindless-texture.glsl"

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

layout(binding = 0, rgba16f) uniform writeonly image2D u_velocity_texture;

layout(push_constant) uniform PushConstantData {
    mat4 inv_VP;
    mat4 prev_VP;

    vec2 resolution;
    vec2 inv_resolution;

    uint depth_texture_index;
    uint view_normal_depth_texture_index;
    uint _padding[2];
};

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);
    if (any(greaterThanEqual(id, resolution)))
        return;

    float depth = sample_texel(depth_texture_index, id, 0).r;

    vec2 uv = (id + 0.5) * inv_resolution;

    vec3 current_ndc_pos = vec3(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth);
    vec3 world_pos = ndc_pos_to_world_pos(current_ndc_pos, inv_VP);

    vec4 prev_ndc_pos = prev_VP * vec4(world_pos, 1.0f);
    prev_ndc_pos.xyz /= prev_ndc_pos.w;

    vec3 view_normal = octahedral_decode(sample_texel(view_normal_depth_texture_index, id, 0).zw);

    const float c1 = 0.003;
    const float c2 = 0.017;
    float epsilon = c1 + c2 * abs(view_normal.z);

    float depth_diff = abs(1.0f - (prev_ndc_pos.z / current_ndc_pos.z));
    vec2 visibility_motion = depth_diff < epsilon ? (current_ndc_pos.xy - prev_ndc_pos.xy) : vec2(-1.0f);
    imageStore(u_velocity_texture, id, vec4(visibility_motion * 100.0f, 0.0f, 0.0f));
}