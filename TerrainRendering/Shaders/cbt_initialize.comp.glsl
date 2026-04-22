#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(std430, set = 0, binding = 0) buffer cbtTree {
    uint heap[];
};

layout(std430, set = 0, binding = 1) buffer CBTDispatchIndirect {
    uint dispatch_count[];
};

#define CBT_ENABLE_WRITE
#include "cbt.glsl"

layout(push_constant) uniform PushConstant {
    uint depth;
    int initDepth;
};

void main() {
    // Get total no of  buffer in u32
    uint bufferCount = cbt_HeapUint32Size(depth);
    for (int i = 0; i < bufferCount; ++i) {
        heap[i] = 0;
    }
    heap[0] = 1 << depth;

    uint minID = 1 << initDepth;
    uint maxID = 2 << initDepth;
    for (uint n = minID; n < maxID; ++n) {
        cbt_Node node = cbt_CreateNode(n, initDepth);
        cbt_HeapWrite_BitField(node, 1u);
    }

    uint totalLeaves = maxID - minID;
    dispatch_count[0] = (totalLeaves + 255) >> 8;
    dispatch_count[1] = 1;
    dispatch_count[2] = 1;
}