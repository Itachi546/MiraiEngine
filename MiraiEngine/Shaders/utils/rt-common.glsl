#ifndef RT_COMMON_GLSL
#define RT_COMMON_GLSL

struct MeshInstanceData {
    uint vertex_offset;
    uint index_offset;
    uint vertex_stride;
    uint material_index;
};

struct Vertex {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec2 tex_coord;
};

Vertex fetch_interpolated_vertex(uint triangle_index, MeshInstanceData instance, vec3 bary) {
    uint index_address = instance.index_offset + triangle_index * 3;
    uint i0 = vertices[index_address + 0];
    uint i1 = vertices[index_address + 1];
    uint i2 = vertices[index_address + 2];

    uint va0 = instance.vertex_offset + i0 * instance.vertex_stride;
    uint va1 = instance.vertex_offset + i1 * instance.vertex_stride;
    uint va2 = instance.vertex_offset + i2 * instance.vertex_stride;

    Vertex result;
    result.position = bary.x * unpack_position(va0) + bary.y * unpack_position(va1) + bary.z * unpack_position(va2);
    result.normal = normalize(bary.x * unpack_normal(va0) + bary.y * unpack_normal(va1) + bary.z * unpack_normal(va2));
    result.tangent = normalize(bary.x * unpack_tangent(va0) + bary.y * unpack_tangent(va1) + bary.z * unpack_tangent(va2));
    result.tex_coord = bary.x * unpack_uv(va0) + bary.y * unpack_uv(va1) + bary.z * unpack_uv(va2);
    return result;
}

#endif