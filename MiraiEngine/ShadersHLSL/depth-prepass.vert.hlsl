#include "global-data-def.hlsli"
#include "vertex-utils.hlsli"

[[vk::binding(0, 0)]]
ConstantBuffer<PerFrameData> cb_per_frame_data;

[[vk::binding(1, 0)]]
StructuredBuffer<float4x4> sb_transforms;

[[vk::binding(2, 0)]]
StructuredBuffer<uint> sb_vertices;

[[vk::binding(3, 0)]]
StructuredBuffer<DrawData> sb_draw_datas;

float4 main(uint vertex_id : SV_VERTEXID, [[vk::builtin("DrawIndex")]] uint draw_index : TEXCOORD): SV_Position {
  DrawData draw_data = sb_draw_datas[draw_index];
  uint vertex_address = draw_data.vertex_offset + vertex_id * draw_data.vertex_stride;
  float3 position = unpack_position(sb_vertices, vertex_address);
  float4x4 M = sb_transforms[draw_data.transform_index];
  return mul(cb_per_frame_data.VP, mul(M, float4(position, 1.0f)));
}
