#version 450

layout(location = 0) out vec4 fragColor;

layout(location = 0) in FS_IN
{
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

void main()
{
    vec3 n = normalize(fs_in.normal);
    fragColor = vec4(n * 0.5 + 0.5f, 1.0f);
}