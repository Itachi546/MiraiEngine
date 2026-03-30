#include "cubemap-utils.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float4> u_hdri;

[[vk::binding(0, 0)]]
SamplerState u_sampler;

[[vk::binding(1, 0)]]
RWTexture2DArray<float4> u_cubemap;

struct PushConstantData {
    float2 cubemap_size;
};

[[vk::push_constant]]
PushConstantData pc;

static const float2 inv_atan = float2(0.1591, 0.3183);
float2 spherical_coord(float3 p)
{
    float2 uv = float2(atan2(p.z, p.x), asin(p.y));
    uv *= inv_atan;
    uv += 0.5f;
    return uv;
}

[numthreads(32, 32, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    int3 cube_coord = int3(dispatch_thread_id);

    float3 p = normalize(uv_to_xyz(cube_coord, pc.cubemap_size));

    float2 uv = spherical_coord(p);

    float3 color = u_hdri.SampleLevel(u_sampler, uv, 0).rgb;

    u_cubemap[cube_coord] = float4(color, 1.0f);
}