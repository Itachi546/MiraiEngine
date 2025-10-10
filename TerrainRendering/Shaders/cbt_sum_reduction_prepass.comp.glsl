#version 460

#define LOCAL_WORK_SIZE 256
layout(local_size_x = LOCAL_WORK_SIZE, local_size_y = 1, local_size_z = 1) in;

#extension GL_GOOGLE_include_directive : enable
layout(set = 0, binding = 0) buffer CBTNode {
    uint heap[];
};

layout(push_constant) uniform PushConstant {
    uint u_Level;
};

shared uint sum_reduction_outputs[LOCAL_WORK_SIZE];

#define CBT_ENABLE_WRITE
#include "cbt.glsl"
#include "leb.glsl"

void main() {
    uint threadID = gl_GlobalInvocationID.x;
    uint leafCount = (1 << u_Level);

    uint totalDispatches = leafCount / 32;
    uint prevLevelOffset = totalDispatches;

    // Corrected ID for writing/reading to shared storage
    uint i = threadID % LOCAL_WORK_SIZE;

    if (threadID < totalDispatches) {
        // For base pass, we combine 32 leaf into single dispatch
        uint leafStart = threadID * 32 + leafCount;
        uint bufferID = cbt_GetBitIndex(cbtNode(leafStart, u_Level)) >> 5;
        uint value = heap[bufferID];

        // Copy the value of current level to previous level
        // Current level is modified while updating the subdivision,
        // so instead we backup the current level into previous level as they
        // require same number of bit
        heap[bufferID - prevLevelOffset] = value;

        uint total = bitCount(value);
        sum_reduction_outputs[i] = total;
    } else {
        sum_reduction_outputs[i] = 0;
    }

    // Wait for the write
    barrier();

    // At this point, we collect the output of 16 thread in the shared memory
    // and merge it to write as 3 int (16 * 6 = 96 bits equals to 3 int)
    if (threadID % 16 == 0 && threadID < totalDispatches) {
        // Get buffer represented by this thread group of 16
        uint writeLeafStart = leafCount >> 5;
        uint writeBitIndex = cbt_GetBitIndex(cbtNode(writeLeafStart, u_Level - 6));
        uint writeBufferID = (writeBitIndex >> 5) + 3 * (threadID >> 4);

        uint result = (sum_reduction_outputs[i + 5] << 30) |
                      (sum_reduction_outputs[i + 4] << 24) |
                      (sum_reduction_outputs[i + 3] << 18) |
                      (sum_reduction_outputs[i + 2] << 12) |
                      (sum_reduction_outputs[i + 1] << 6) |
                      sum_reduction_outputs[i];
        heap[writeBufferID++] = result;

        result = (sum_reduction_outputs[i + 10] << 28) |
                 (sum_reduction_outputs[i + 9] << 22) |
                 (sum_reduction_outputs[i + 8] << 16) |
                 (sum_reduction_outputs[i + 7] << 10) |
                 (sum_reduction_outputs[i + 6] << 4) |
                 (sum_reduction_outputs[i + 5] >> 2);
        heap[writeBufferID++] = result;

        result = (sum_reduction_outputs[i + 15] << 26) |
                 (sum_reduction_outputs[i + 14] << 20) |
                 (sum_reduction_outputs[i + 13] << 14) |
                 (sum_reduction_outputs[i + 12] << 8) |
                 (sum_reduction_outputs[i + 11] << 2) |
                 (sum_reduction_outputs[i + 10] >> 4);
        heap[writeBufferID++] = result;
    }
}