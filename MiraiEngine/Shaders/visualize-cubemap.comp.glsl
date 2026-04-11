#version 450

layout(local_size_x = 32, local_size_y = 32, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
#include "utils/raycast.glsl"
#include "utils/color.glsl"
#include "utils/bindless-sampler.glsl"

layout(push_constant) uniform PushConstants {
    mat4 invP;
    mat4 invV;
    vec4 dims;
};

layout(binding = 0) uniform textureCube u_cubemap_texture;
layout(binding = 1, rgba8) uniform image2D u_output_texture;

void main() {
    ivec2 id = ivec2(gl_GlobalInvocationID.xy);

    vec2 uv = vec2(id) / dims.xy;
    vec3 rd = generate_camera_ray(vec2(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y), invP, invV);
    rd.y = -rd.y;
    vec3 col = texture(samplerCube(u_cubemap_texture, u_samplers[SAMPLER_LINEAR_CLAMP]), rd).rgb;
    col = linear_to_srgb(ACESFilm(col));
    imageStore(u_output_texture, id, vec4(col, 1.0f));
}