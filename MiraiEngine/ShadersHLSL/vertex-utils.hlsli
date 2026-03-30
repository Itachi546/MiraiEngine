#ifndef VERTEX_UTILS_HLSLI
#define VERTEX_UTILS_HLSLI

float unpack_u8_to_float(uint x)
{
    return (x - 127.5f) / 127.0f;
}

float3 u32_to_float3(uint data)
{
    float3 result;
    result.x = unpack_u8_to_float((data >> 24) & 0xff);
    result.y = unpack_u8_to_float((data >> 16) & 0xff);
    result.z = unpack_u8_to_float((data >> 8)  & 0xff);
    return result;
}

float3 unpack_position(StructuredBuffer<uint> buffer, uint base_address)
{
    return float3(
        asfloat(buffer[base_address]),
        asfloat(buffer[base_address + 1]),
        asfloat(buffer[base_address + 2]));
}

float3 unpack_normal(StructuredBuffer<uint> buffer, uint base_address)
{
    return normalize(u32_to_float3(buffer[base_address + 3]));
}

float3 unpack_tangent(StructuredBuffer<uint> buffer, uint base_address)
{
    return normalize(u32_to_float3(buffer[base_address + 4]));
}

float3 unpack_bitangent(StructuredBuffer<uint> buffer, uint base_address)
{
    return normalize(u32_to_float3(buffer[base_address + 5]));
}

float2 unpack_uv(StructuredBuffer<uint> buffer, uint base_address)
{
    return float2(
        asfloat(buffer[base_address + 6]),
        asfloat(buffer[base_address + 7]));
}

uint4 unpack_joints(StructuredBuffer<uint> buffer, uint base_address)
{
    uint joints = buffer[base_address + 8];
    return uint4(
        (joints >> 24) & 0xff,
        (joints >> 16) & 0xff,
        (joints >> 8)  & 0xff,
        joints         & 0xff);
}

float4 unpack_weights(StructuredBuffer<uint> buffer, uint base_address)
{
    return float4(
        asfloat(buffer[base_address + 9]),
        asfloat(buffer[base_address + 10]),
        asfloat(buffer[base_address + 11]),
        asfloat(buffer[base_address + 12]));
}

#endif