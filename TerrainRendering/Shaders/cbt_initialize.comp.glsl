#version 460

#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer cbtTree {
    uint heap[];
};

#include "cbt.glsl"

layout(push_constant) uniform PushConstant {
    uint depth;
    uint initDepth;
};

void main() {
    // Get total no of  buffer in u32
    uint bufferCount = cbt_GetAllocationSizeU32(depth);
    for (int i = 0; i < bufferCount; ++i) {
        heap[i] = 0;
    }

    uint minID = 1 << initDepth;
    uint maxID = 2 << (initDepth);

    for (uint n = minID; n < maxID; ++n) {
        cbtNode node = {n, initDepth};
        cbt_WriteBitField(node, 1u);
    }

    heap[0] = depth;
}