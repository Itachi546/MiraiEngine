#include "transform.hlsli"

[[vk::binding(0, 0)]]
Texture2D<float> u_depth_texture;

[[vk::binding(1, 0)]]
RWTexture2D<float4> u_output_texture;

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
    int2 uv = int2(dispatch_thread_id.xy);
    if(uv.x > (int)pc.width - 1 || uv.y > (int)pc.height - 1)
        return;
    
    float depth = u_depth_texture.Load(int3(uv, 0));
    float linear_depth = linearize_depth(depth, pc.znear, pc.zfar) * 0.05;
    u_output_texture[uv] = float4(linear_depth, linear_depth, linear_depth, 1.0);
}