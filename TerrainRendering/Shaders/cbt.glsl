#ifndef CBT_GLSL
#define CBT_GLSL

struct cbtNode {
    uint id;
    uint depth;
};

uint cbt_MaxDepth(uint data) {
    return findLSB(data);
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

cbtNode cbt_HeapToBitIndex(cbtNode node) {
    uint maxDepth = cbt_MaxDepth(heap[0]);
    cbtNode leaf;
    leaf.id = node.id * (1 << (maxDepth - node.depth));
    leaf.depth = maxDepth;
    return leaf;
}

uint cbt_GetBitValue(uint bufferID, uint bitID) {
    return (bufferID >> bitID) & 1u;
}

#ifdef CBT_ENABLE_WRITE
// Only used to update the state of leaf
void cbt_SetBitValue(uint bufferID, uint bitID, uint value) {
    uint bitMask = ~(1u << bitID);

    // Clear the value at the location
    atomicAnd(heap[bufferID], bitMask);

    // Set the value at the location
    atomicOr(heap[bufferID], value << bitID);
}
// Used to write directly to the leaf, only 1 bit
void cbt_WriteBitField(cbtNode node, uint value) {
    // @TODO refactor this
    cbtNode leafNode = cbt_HeapToBitIndex(node);
    uint bitIndex = cbt_GetBitIndex(leafNode);
    // Divide bitIndex by 32 to get the index in uint8 array
    // Calculate the remainder when divided by 31
    cbt_SetBitValue(bitIndex >> 5, bitIndex & 31, value);
}
#endif

// Used to read the value of leaf node, only 1 bit
uint cbt_ReadBitField(cbtNode node, uint value) {
    cbtNode leafNode = cbt_HeapToBitIndex(node);
    uint bitIndex = cbt_GetBitIndex(leafNode);
    return cbt_GetBitValue(bitIndex >> 5, bitIndex & 31);
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
/*
uint heapRead(cbtNode leftChild, uint maxDepth) {
    if (leftChild.depth < maxDepth - 5)
        return cbt_HeapRead(leftChild);
    uint numBits = maxDepth - leftChild.depth;
    //uint firstLeaf = leftChild.id << 5;
    //uint bufferIndex = cbt_GetBitIndex(firstLeaf) >> 5;
    if (numBits == 5)
        return 16;
    else if (numBits == 4)
        return 8;
    else if (numBits == 3)
        return 4;
    else if (numBits == 2)
        return 2;
    else
        return 1;
}
*/

// Node index can be in between 1 and 63
uint getBitCount(cbtNode node, uint value) {
    if (node.depth == 0)
        return bitCount(value);
    // Given a relative node index (only depth 5) and value (u32) we calculate the bitCount for that node
    // Calculate total no of bit required for this depth
    uint numBits = 1 << (5 - node.depth);
    // Calculate first child at this depth
    uint firstChild = 1 << node.depth;
    // Calculate relative distance of child at that level
    uint childOffset = node.id - firstChild;

    // Create numBit mask to mask out the value
    uint mask = (1 << numBits) - 1u;

    mask = mask << (childOffset * numBits);
    return bitCount(value & mask);
}

uint getBufferValueFiner(cbtNode node, uint maxDepth) {
    uint firstLeaf = node.id << (maxDepth - node.depth);
    uint prevLevelOffset = (1 << maxDepth) >> 5;
    uint bufferIndex = (cbt_GetBitIndex(cbtNode(firstLeaf, maxDepth)) >> 5) - prevLevelOffset;
    return heap[bufferIndex];
}

uint heapRead(cbtNode node, uint maxDepth) {
    if (node.depth < maxDepth - 4)
        return cbt_HeapRead(node);

    uint value = getBufferValueFiner(node, maxDepth);

    cbtNode temp;
    temp.depth = 5 - (maxDepth - node.depth);
    uint parent = node.id >> temp.depth;
    temp.id = node.id - (parent << temp.depth) + (1 << (temp.depth));
    return getBitCount(temp, value);
}
/*
// Node is always leftChild
cbtNode cbt_BinarySearchFiner(cbtNode node, uint nodeID) {
    // Start a new binary search within u32, we don't store the sum reduction for last
    // 5 depth, so we have to runtime calculation
    uint value = getBufferValueFiner(node);
    cbtNode temp;
    temp.id = 1;
    temp.depth = 0u;
    while (getBitCount(temp, value) > 1u) {
        cbtNode leftChild;
        leftChild.id = temp.id << 1;
        leftChild.depth = temp.depth + 1;

        uint cmp = getBitCount(leftChild, value);
        uint b = nodeID < cmp ? 0u : 1u;
        temp = leftChild;
        temp.id |= b;
        nodeID -= cmp * b;
    }
    // Find the child index at given depth
    // We calculate the childIndex relative to current coarse node, by using
    // relative distance
    uint tempNodeLeafStart = 1 << temp.depth;
    node.id = (node.id << temp.depth) + (temp.id - tempNodeLeafStart);
    node.depth += temp.depth;
    return node;
}
*/
cbtNode cbt_BinarySearch(uint nodeID) {
    cbtNode node;
    node.id = 1u;
    node.depth = 0u;

    uint maxDepth = cbt_MaxDepth(heap[0]);
    while (heapRead(node, maxDepth) > 1u) {
        cbtNode leftChild;
        leftChild.id = node.id << 1;
        leftChild.depth = node.depth + 1;

        uint cmp = heapRead(leftChild, maxDepth);
        uint b = nodeID < cmp ? 0u : 1u;
        node = leftChild;
        node.id |= b;
        nodeID -= cmp * b;
    }
    return node;
}

bool cbt_IsCeilNode(cbtNode node) {
    return node.depth == cbt_MaxDepth(heap[0]);
}

bool cbt_IsRootNode(cbtNode node) {
    return node.id == 1u;
}

bool cbt_IsNullNode(in const cbtNode node) {
    return (node.id == 0u);
}

cbtNode cbt_ParentNode(cbtNode node) {
    if (cbt_IsNullNode(node))
        return node;
    return cbtNode(node.id >> 1, node.depth - 1);
}

cbtNode cbt_RightSiblingNode(cbtNode node) {
    if (cbt_IsNullNode(node))
        return node;
    return cbtNode(node.id | 1, node.depth);
}

cbtNode cbt_RightChildNode(cbtNode node) {
    if (cbt_IsNullNode(node))
        return node;
    return cbtNode((node.id << 1) | 1u, node.depth + 1);
}

#ifdef CBT_ENABLE_WRITE

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

void cbt_MergeNode(cbtNode node) {
    if (cbt_IsRootNode(node))
        return;
    cbtNode rightSibling = cbt_RightSiblingNode(node);
    cbt_WriteBitField(rightSibling, 0u);
}

void cbt_SplitNode(cbtNode node) {
    if (cbt_IsCeilNode(node))
        return;

    cbtNode rightChild = cbt_RightChildNode(node);
    cbt_WriteBitField(rightChild, 1);
}
#endif

#endif