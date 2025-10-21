#pragma once

#include <stdint.h>

struct cbtTree {
    uint64_t *heap;
};

struct cbtNode {
    uint64_t leafIndex : 58;
    uint64_t depth : 6;
};

void cbt_Allocate(cbtTree *tree, uint64_t depth);
void cbt_Initialize(cbtTree *tree, uint64_t depth);
uint64_t cbt_GetAllocationSizeByte(uint64_t depth);
uint64_t cbt_NumLeaves(uint64_t depth);

#ifdef CBT_IMPLEMENTATION
// The acutal no of bytes required is
// uint32_t numBytes = ((1 << (this->cbtDepth + 2)) - (this->cbtDepth + 3)) / 8;
// But we are going to use the approximate
uint64_t cbt_GetAllocationSizeByte(uint64_t depth) {
    return 1ull << (depth - 1ull);
}

void cbt_Initialize(cbtTree* tree, uint64_t depth) {
    uint64_t minNodeID = 1ull << depth;
    uint64_t maxNodeID = 2ull << depth;

    for(uint64_t i = minNodeID; i < maxNodeID; ++i) {

    }
}

void cbt_Allocate(cbtTree *tree, uint64_t depth) {
    ASSERT_MSG(depth >= 5, "Tree Depth must be at least 5");
    ASSERT_MSG(depth <= 58, "Tree Depth must be at most 58");

    uint64_t allocationSize = cbt_GetAllocationSizeByte(depth);

    uint8_t *heap = new uint8_t[allocationSize];
    tree->heap = reinterpret_cast<uint64_t *>(heap);
    tree->heap[0] = 1ull << depth;
    cbt_Initialize(tree, depth);
}

uint64_t cbt_NumLeaves(uint64_t depth) {
    return 1ull << depth;
}

uint64_t cbt_GetBitIndex(uint64_t leafIndex, uint64_t depth) {
    uint64_t dk = cast_u32(std::log2(leafIndex));
    uint64_t ndk = depth - dk + 1;
    return (2ull << dk) + leafIndex * ndk;
}

void cbt_SetValue(uint64_t leafIndex, uint64_t depth, uint64_t val) {
    uint64_t bitIndex = cbt_GetBitIndex(leafIndex, depth);
}
#endif