#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require

#define DISABLE_TEXTURE_DERIVATIVE
#include "../utils/bindless-texture.glsl"
#include "../utils/bindless-sampler.glsl"
#include "../utils/material.glsl"

layout(location = 0) rayPayloadInEXT vec4 hitColor;
hitAttributeEXT vec2 bary_coord;

struct MeshInstanceData {
    uint vertex_offset;
    uint index_offset;
    uint vertex_stride;
    uint material_index;
};

layout(set = 0, binding = 2) readonly buffer MeshInstanceBuffer {
    MeshInstanceData mesh_instances[];
};

layout(set = 0, binding = 3) readonly buffer VertexBuffer {
    uint vertices[];
};

layout(set = 0, binding = 4) readonly buffer MaterialBuffer {
    PBRMaterial materials[];
};

#include "../utils/vertexdata.glsl"
struct Vertex {
    vec3 position;
    vec3 normal;
    vec3 tangent;
    vec2 tex_coord;
};

Vertex fetch_interpolated_vertex(uint triangle_index, MeshInstanceData instance, vec3 bary_coord) {
    uint index_address = instance.index_offset + triangle_index * 3;
    uint i0 = vertices[index_address + 0];
    uint i1 = vertices[index_address + 1];
    uint i2 = vertices[index_address + 2];

    uint va0 = instance.vertex_offset + i0 * instance.vertex_stride;
    uint va1 = instance.vertex_offset + i1 * instance.vertex_stride;
    uint va2 = instance.vertex_offset + i2 * instance.vertex_stride;

    Vertex result;
    vec3 p0 = unpack_position(va0);
    vec3 n0 = unpack_normal(va0);
    vec3 t0 = unpack_tangent(va0);
    vec2 uv0 = unpack_uv(va0);

    vec3 p1 = unpack_position(va1);
    vec3 n1 = unpack_normal(va1);
    vec3 t1 = unpack_tangent(va1);
    vec2 uv1 = unpack_uv(va1);

    vec3 p2 = unpack_position(va2);
    vec3 n2 = unpack_normal(va2);
    vec3 t2 = unpack_tangent(va2);
    vec2 uv2 = unpack_uv(va2);

    result.position = bary_coord.x * p0 + bary_coord.y * p1 + bary_coord.z * p2;
    result.normal = normalize(bary_coord.x * n0 + bary_coord.y * n1 + bary_coord.z * n2);
    result.tangent = normalize(bary_coord.x * t0 + bary_coord.y * t1 + bary_coord.z * t2);
    result.tex_coord = bary_coord.x * uv0 + bary_coord.y * uv1 + bary_coord.z * uv2;

    return result;
}

void main() {
    // Color using barycentric coordinates
    MeshInstanceData instance = mesh_instances[gl_InstanceCustomIndexEXT];
    uint triangle_index = gl_PrimitiveID;

    Vertex vertex = fetch_interpolated_vertex(triangle_index, instance, vec3(1.0 - bary_coord.x - bary_coord.y, bary_coord.x, bary_coord.y));

    PBRMaterial material = materials[instance.material_index];
    vec2 texture_scale = vec2(material.texture_scale_x, material.texture_scale_y);

    vec4 albedo = fetch_albedo(material, vertex.tex_coord * texture_scale, 0.0f);
    vec3 emissive = fetch_emissive(material, vertex.tex_coord * texture_scale, 0.0f);
    vec3 sn = fetch_normal_map(material, vertex.tex_coord * texture_scale, 0.0f);

    vec3 n = vertex.normal;
    vec3 t = vertex.tangent;
    vec3 bt = cross(n, t);

    vec3 detail_normal = normalize(sn.x * t + sn.y * bt + sn.z * n);

    hitColor = vec4(emissive + albedo.rgb, 0.0f);
}