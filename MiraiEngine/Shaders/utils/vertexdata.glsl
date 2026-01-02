#ifndef VERTEX_DATA_GLSL
#define VERTEX_DATA_GLSL

struct Vertex
{
    float px, py, pz;
    uint normal;

    uint tangent;
    uint bitangent;

    float tu, tv;
};

struct DrawData {
    uint transform_index;
    uint material_index;
    uint _padding[2];
};

float unpack_u8_to_float(uint x)
{
    return (x - 127.5f) / (127.0f);
}

vec3 u32_to_vec3(uint data)
{
    vec3 result;
    result.x = unpack_u8_to_float((data >> 24) & 0xff);
    result.y = unpack_u8_to_float((data >> 16) & 0xff);
    result.z = unpack_u8_to_float((data >> 8) & 0xff);
    return result;
}

#endif