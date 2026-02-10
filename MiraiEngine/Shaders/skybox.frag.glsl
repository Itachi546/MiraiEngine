#version 450

#extension GL_GOOGLE_include_directive : enable
#include "utils/raycast.glsl"
#include "utils/color.glsl"

layout(location = 0) in vec2 uv;

layout(location = 0) out vec4 fragColor;

layout(push_constant) uniform PushConstants {
    mat4 invP;
    mat4 invV;
};

layout(set = 0, binding = 0) uniform samplerCube u_cubemap;

void main() {
    vec3 rd = generate_camera_ray(vec2(uv.x * 2.0f - 1.0f, 1.0f - 2.0f * uv.y), invP, invV);
    rd.y = -rd.y;
    vec3 col = texture(u_cubemap, rd).rgb;
    fragColor = vec4(col, 1.0f);
}