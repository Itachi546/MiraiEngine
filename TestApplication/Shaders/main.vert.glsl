#version 460

vec2 positions[6] = vec2[](
    vec2(1.0f, 1.0f),
    vec2(-1.0f, -1.0f),
    vec2(-1.0f, 1.0f),

    vec2(-1.0f, -1.0f),
    vec2(1.0f, 1.0f),
    vec2(1.0f, -1.0f));

layout(location = 0) out vec2 uv;

layout(set = 0, binding = 0) readonly buffer Positions
{
    vec3 vPosition[];
};

layout(push_constant) uniform PushConstant
{
    mat4 VP;
    mat4 P;
    mat4 V;
    vec4 camera_position;
};

void main()
{
    mat4 m = VP * P * V;
    vec2 position = mat2(m) * positions[gl_VertexIndex];
    gl_Position = vec4(position + camera_position.xz, 0.0f, 1.0f);

    uv = position;
}