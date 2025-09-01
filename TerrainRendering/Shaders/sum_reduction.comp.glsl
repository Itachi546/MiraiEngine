#version 460

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable

layout(set = 0, binding = 0) buffer CBTNode {
    uint heap[];
};

layout(push_constant) uniform PushConstant {
    uint u_Level;
};

#include "cbt.glsl"

void main() {
    uint threadID = gl_GlobalInvocationID.x;
    uint cnt = (1 << u_Level);
    if (threadID < cnt) {
        uint nodeID = threadID + cnt;
        uint x0 = cbt_HeapRead(cbtNode(nodeID << 1, u_Level + 1));
        uint x1 = cbt_HeapRead(cbtNode(nodeID << 1 | 1u, u_Level + 1));
        cbt_HeapWrite(cbtNode(nodeID, u_Level), x0 + x1);
    }
}