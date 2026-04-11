#version 450

#extension GL_GOOGLE_include_directive : enable
#include "../utils/raycast.glsl"
#include "../utils/color.glsl"
#include "../utils/bindless-sampler.glsl"

layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 invP;
    mat4 invV;
};

layout(set = 0, binding = 0) uniform textureCube u_cubemap;

void main() {
    vec3 rd = generate_camera_ray(vec2(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y), invP, invV);
    rd.y = -rd.y;
    vec3 col = texture(samplerCube(u_cubemap, u_samplers[SAMPLER_LINEAR_CLAMP]), rd).rgb;
    fragColor = vec4(linear_to_srgb(ACESFilm(col)), 1.0f);
}