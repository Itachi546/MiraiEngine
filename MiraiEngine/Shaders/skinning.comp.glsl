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
const uint OUTPUT_VERTEX_SIZE = 8;

layout(set = 0, binding = 0) buffer VertexBinding {
    uint vertices[];
};

layout(set = 0, binding = 2) readonly buffer MatrixPallets {
    mat4 matrix_palletes[];
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

    mat4 skinning_matrix = matrix_palletes[matrix_pallete_offset + joints.x] * weights.x;
    skinning_matrix += matrix_palletes[matrix_pallete_offset + joints.y] * weights.y;
    skinning_matrix += matrix_palletes[matrix_pallete_offset + joints.z] * weights.z;
    skinning_matrix += matrix_palletes[matrix_pallete_offset + joints.w] * weights.w;

    uint output_address = output_offset + id * OUTPUT_VERTEX_SIZE;
    uvec3 out_position = floatBitsToUint(vec3(skinning_matrix * vec4(position, 1.0f)));

    uint ptr = output_offset + id * OUTPUT_VERTEX_SIZE;

    vertices[ptr] = out_position.x;
    vertices[ptr + 1] = out_position.y;
    vertices[ptr + 2] = out_position.z;

    mat3 normal_matrix = mat3(transpose(inverse(skinning_matrix)));
    vertices[ptr + 3] = pack_vec3_to_u32(normalize(normal_matrix * unpack_normal(vertex_address)));
    vertices[ptr + 4] = pack_vec3_to_u32(normalize(normal_matrix * unpack_tangent(vertex_address)));
    vertices[ptr + 5] = pack_vec3_to_u32(normalize(normal_matrix * unpack_bitangent(vertex_address)));

    uvec2 uv = floatBitsToUint(unpack_uv(vertex_address));

    vertices[ptr + 6] = uv.x;
    vertices[ptr + 7] = uv.y;
}