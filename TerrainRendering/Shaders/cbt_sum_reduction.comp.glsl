#version 460

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
layout(set = 0, binding = 0) buffer CBTNode {
    uint heap[];
};

layout(set = 0, binding = 1) buffer DrawIndirectCommand {
    uint vertexCount;
    uint instanceCount;
    uint firstVertex;
    uint firstInstance;
};

layout(set = 0, binding = 2) buffer CBTDispatchIndirect {
    uint dispatch_count[];
};

layout(push_constant) uniform PushConstant {
    int u_Level;
};

#define CBT_ENABLE_WRITE
#include "cbt.glsl"
#include "leb.glsl"

void main() {
    uint threadID = gl_GlobalInvocationID.x;
    uint cnt = (1 << (u_Level));
    if (threadID < cnt) {
        uint nodeID = threadID + cnt;
        uint x0 = cbt_HeapRead(cbt_CreateNode(nodeID << 1, u_Level + 1));
        uint x1 = cbt_HeapRead(cbt_CreateNode((nodeID << 1) | 1u, u_Level + 1));
        cbt_HeapWrite(cbt_CreateNode(nodeID, u_Level), x0 + x1);
        if (u_Level == 0) {
            uint totalLeaves = x0 + x1;
            dispatch_count[0] = (totalLeaves + 255) >> 8;
            vertexCount = 3;
            instanceCount = totalLeaves;
            firstInstance = 0;
            firstVertex = 0;
        }
    }
}