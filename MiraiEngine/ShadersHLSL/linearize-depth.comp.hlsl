#include "transform.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> u_depth_texture;

[[vk::binding(1, 0)]]
RWTexture2D<float4> u_output_texture;

[[vk::binding(0, 1)]]
SamplerState u_samplers[];

struct PushConstantData {
    uint width;
    uint height;
    uint znear;
    uint zfar;   
};

[[vk::push_constant]]
PushConstantData pc;

[numthreads(32, 32, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID) {
    int2 iuv = int2(dispatch_thread_id.xy);
    if(iuv.x >= (int)pc.width || iuv.y >= (int)pc.height)
        return;
    
    //float2 uv = float2(iuv.x, iuv.y) / float2(pc.width, pc.height);
    //float depth = u_depth_texture.SampleLevel(u_samplers[0], iuv, 0);
    float linear_depth = 1.0;//linearize_depth(depth, pc.znear, pc.zfar) * 0.05;
    u_output_texture[iuv] = float4(linear_depth, 0, linear_depth, 1.0);
}