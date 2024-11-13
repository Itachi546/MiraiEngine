#version 450

#extension GL_GOOGLE_include_directive : enable

layout(location = 0) out vec4 fragColor;

layout(location = 0) in FS_IN {
    vec3 normal;
    vec3 tangent;
    vec3 bitangent;
    vec3 worldPos;
    vec3 lsPos;
    vec3 viewDir;
    vec2 uv;
    flat uint matId;
}
fs_in;

#include "utils/bindless.glsl"

void main() {
    vec3 n = normalize(fs_in.normal);
    vec3 col = vec3(0.0f);
    vec4 albedo = sample_texture(16, fs_in.uv);
    col += dot(n, normalize(vec3(-1.0f, -1.0f, 1.0f))) * albedo.rgb;
    fragColor = vec4(col.rgb, 1.0f);
}