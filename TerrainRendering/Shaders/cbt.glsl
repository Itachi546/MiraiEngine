#ifndef CBT_GLSL
#define CBT_GLSL

struct cbtNode {
    uint id;
    uint depth;
};

uint cbt_MaxDepth(uint data) {
    return findMSB(data);
}

uint cbt_GetAllocationSizeByte(uint depth) {
    return 1 << (depth - 1);
}

uint cbt_GetAllocationSizeU32(uint depth) {
    return 1 << (depth - 3);
}

uint cbt_GetBitIndex(cbtNode node) {
    // Given a node, find the position of it's leafIndex
    uint ndk = cbt_MaxDepth(heap[0]) - node.depth + 1;
    return (2 << node.depth) + node.id * ndk;
}

// Only used to update the state of leaf
void cbt_SetBitValue(uint bufferID, uint bitID, uint value) {
    uint bitMask = ~(1u << bitID);

    // Clear the value at the location
    atomicAnd(heap[bufferID], bitMask);

    // Set the value at the location
    atomicOr(heap[bufferID], value << bitID);
}

uint cbt_GetBitValue(uint bufferID, uint bitID) {
    return (bufferID >> bitID) & 1u;
}

// Used to write directly to the leaf, only 1 bit
void cbt_WriteBitField(cbtNode node, uint value) {
    uint bitIndex = cbt_GetBitIndex(node);
    // Divide bitIndex by 32 to get the index in uint8 array
    // Calculate the remainder when divided by 31
    cbt_SetBitValue(bitIndex >> 5, bitIndex & 31, value);
}

// Used to read the value of leaf node, only 1 bit
uint cbt_ReadBitField(cbtNode node, uint value) {
    uint bitIndex = cbt_GetBitIndex(node);
    return cbt_GetBitValue(bitIndex >> 5, bitIndex & 31);
}

void cbt_BitFieldInsert(uint bufferID, uint bitOffset, uint bitCount, uint bitData) {
    uint bitMask = ~(~(0xFFFFFFFFu << bitCount) << bitOffset);
    atomicAnd(heap[bufferID], bitMask);
    atomicOr(heap[bufferID], bitData << bitOffset);
}

// Used to write n-bit in the heap while
// updating sumReduction
void cbt_HeapWrite(cbtNode node, uint bitData) {
    uint bitCount = cbt_MaxDepth(heap[0]) - node.depth + 1;
    uint alignedBitOffset = cbt_GetBitIndex(node);
    uint maxHeapIndex = cbt_GetAllocationSizeU32(heap[0]);
    uint heapIndexLSB = alignedBitOffset >> 5u;
    uint heapIndexMSB = min(heapIndexLSB + 1, maxHeapIndex);

    uint bitOffsetLSB = alignedBitOffset & 31;
    uint bitCountLSB = min(32 - bitOffsetLSB, bitCount);
    uint bitCountMSB = bitCount - bitCountLSB;

    cbt_BitFieldInsert(heapIndexLSB, bitOffsetLSB, bitCountLSB, bitData);
    cbt_BitFieldInsert(heapIndexMSB, 0u, bitCountMSB, bitData >> bitCountLSB);
}

uint cbt_BitFieldExtract(uint bitField, uint bitOffset, uint bitCount) {
    uint bitMask = ~(0xFFFFFFFFu << bitCount);
    return (bitField >> bitOffset) & bitMask;
}

uint cbt_HeapRead(cbtNode node) {
    uint bitCount = cbt_MaxDepth(heap[0]) - node.depth + 1;
    uint alignedBitOffset = cbt_GetBitIndex(node);
    uint maxHeapIndex = cbt_GetAllocationSizeU32(heap[0]);
    uint heapIndexLSB = alignedBitOffset >> 5u;
    uint heapIndexMSB = min(heapIndexLSB + 1, maxHeapIndex);

    uint bitOffsetLSB = alignedBitOffset & 31;
    uint bitCountLSB = min(32 - bitOffsetLSB, bitCount);
    uint bitCountMSB = bitCount - bitCountLSB;

    uint lsb = cbt_BitFieldExtract(heap[heapIndexLSB], bitOffsetLSB, bitCountLSB);
    uint msb = cbt_BitFieldExtract(heap[heapIndexMSB], 0, bitCountMSB);
    return lsb | (msb << bitCountLSB);
}

#endif
