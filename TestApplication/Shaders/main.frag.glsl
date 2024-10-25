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
    fragColor = vec4(normalize(fs_in.normal), 1.0f);
}