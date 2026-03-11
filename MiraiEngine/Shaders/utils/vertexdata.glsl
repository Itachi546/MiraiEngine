#ifndef VERTEX_DATA_GLSL
#define VERTEX_DATA_GLSL
/*
struct Vertex {
    float px, py, pz;
    uint normal;

    uint tangent;
    uint bitangent;

    float tu, tv;
};

struct SkinnedVertex {
    float px, py, pz;
    uint normal;

    uint tangent;
    uint bitangent;

    float tu;
    float tv;

    uint joints;
    float w0, w1, w2, w3;
};
*/

struct DrawData {
    uint transform_index;
    uint material_index;
    uint vertex_offset;
    uint vertex_stride;
};

float unpack_u8_to_float(uint x) {
    return (x - 127.5f) / (127.0f);
}

vec3 u32_to_vec3(uint data) {
    vec3 result;
    result.x = unpack_u8_to_float((data >> 24) & 0xff);
    result.y = unpack_u8_to_float((data >> 16) & 0xff);
    result.z = unpack_u8_to_float((data >> 8) & 0xff);
    return result;
}

vec3 unpack_position(uint address) {
    return vec3(
        uintBitsToFloat(vertices[address]),
        uintBitsToFloat(vertices[address + 1]),
        uintBitsToFloat(vertices[address + 2]));
}

vec3 unpack_normal(uint address) {
    return normalize(u32_to_vec3(vertices[address + 3]));
}

vec3 unpack_tangent(uint address) {
    return normalize(u32_to_vec3(vertices[address + 4]));
}

vec3 unpack_bitangent(uint address) {
    return normalize(u32_to_vec3(vertices[address + 5]));
}

vec2 unpack_uv(uint address) {
    return vec2(
        uintBitsToFloat(vertices[address + 6]),
        uintBitsToFloat(vertices[address + 7]));
}

uvec4 unpack_joints(uint address) {
    uint joints = vertices[address + 7];
    return uvec4(
        (joints >> 24) & 0xff,
        (joints >> 16) & 0xff,
        (joints >> 8) & 0xff,
        joints & 0xff);
}

vec4 unpack_weights(uint address) {
    return vec4(
        uintBitsToFloat(vertices[address + 8]),
        uintBitsToFloat(vertices[address + 9]),
        uintBitsToFloat(vertices[address + 10]),
        uintBitsToFloat(vertices[address + 11]));
}

#endif