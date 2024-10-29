#version 460

layout(location = 0) out VS_OUT
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
vs_out;

#extension GL_GOOGLE_include_directive : enable
#extension GL_ARB_shader_draw_parameters : enable

#include "utils/vertexdata.glsl"

layout(set = 0, binding = 0) readonly buffer VertexData
{
    Vertex vertices[];
};

layout(set = 0, binding = 1) readonly buffer Transform
{
    mat4 transforms[];
};

layout(set = 1, binding = 0) uniform PerFrameData
{
    mat4 P;
    mat4 V;
    mat4 VP;

    vec3 camera_position;
    float elapsed_time;

    vec2 window_size;
    vec2 _padding;
};

void main()
{
    Vertex vertex = vertices[gl_VertexIndex];
    mat4 M = transforms[0];

    vec3 position = vec3(vertex.px, vertex.py, vertex.pz);
    vec4 worldPos = M * vec4(position, 1.0f);
    gl_Position = VP * worldPos;

    vs_out.normal = u32_to_vec3(vertex.normal);
    vs_out.uv = vec2(vertex.tu, vertex.tv);
}