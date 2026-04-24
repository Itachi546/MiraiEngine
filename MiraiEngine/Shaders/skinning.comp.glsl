#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;
struct Vertex {
    vec3 position;
    uint normal;

    uint tangent;
    uint bitangent;
    float tu, tv;
};
const uint OUTPUT_VERTEX_SIZE = 32;

layout(set = 0, binding = 0) readonly buffer VertexBinding {
    uint vertices[];
};

layout(set = 0, binding = 2) readonly buffer MatrixPallets {
    mat4 matrix_palletes[];
};

layout(set = 0, binding = 2) buffer SkinnedVertexOutput {
    Vertex out_vertices[];
};

#include "utils/vertexdata.glsl"

layout(push_constant) uniform PushConstants {
    uint vertex_base_address;
    uint vertex_stride;
    uint vertex_count;
    uint output_offset;

    uint matrix_pallete_offset;
    uint _padding[3];
};

void main() {
    uint id = gl_GlobalInvocationID.x;

    if (id >= vertex_count)
        return;

    uint vertex_address = vertex_base_address + id * vertex_stride;
    vec3 position = unpack_position(vertex_address);
    uvec4 joints = unpack_joints(vertex_address);
    vec4 weights = unpack_weights(vertex_address);

    mat4 skinned_matrix = matrix_palletes[joints.x] * weights.x;
    skinned_matrix += matrix_palletes[joints.y] * weights.y;
    skinned_matrix += matrix_palletes[joints.z] * weights.z;
    skinned_matrix += matrix_palletes[joints.w] * weights.w;

    uint output_address = output_offset + id * OUTPUT_VERTEX_SIZE;
    out_vertices[id].position = vec3(skinned_matrix * vec4(position, 1.0f));

    mat3 normal_matrix = mat3(transpose(inverse(skinned_matrix)));
    out_vertices[id].normal = pack_vec3_to_u32(normal_matrix * unpack_normal(vertex_address));
    out_vertices[id].tangent = pack_vec3_to_u32(normal_matrix * unpack_tangent(vertex_address));
    out_vertices[id].bitangent = pack_vec3_to_u32(normal_matrix * unpack_bitangent(vertex_address));

    vec2 uv = unpack_uv(vertex_address);
    out_vertices[id].tu = uv.x;
    out_vertices[id].tv = uv.y;
}