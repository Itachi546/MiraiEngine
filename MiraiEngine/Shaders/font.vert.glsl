#version 460

layout(set = 0, binding = 0) readonly buffer VertexData {
    vec4 vertices[];
};

layout(push_constant) uniform PushConstants {
    mat4 ortho_matrix;
    uint texture_id;
    uint _unused[3];
};

layout(location = 0) out flat uint tex_id;
layout(location = 1) out vec2 uv;

void main() {
    vec4 vertex = vertices[gl_VertexIndex];

    vec4 position = ortho_matrix * vec4(vertex.xy, 0.0, 1.0);
    gl_Position = vec4(position.xy, 0.0, 1.0);

    tex_id = texture_id;
    uv = vertex.zw;
}