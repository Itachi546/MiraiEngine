#ifndef LEB_GLSL
#define LEB_GLSL

#include "cbt.glsl"

mat3 GetSquareMatrix(uint splitBit) {
    float b = float(splitBit);
    float c = 1.0f - b;
    // return mat3([ c, 0, b ], [ b, c, b ], [ b, 0, c ]);
    return transpose(mat3(
        c, 0, b,
        b, c, b,
        b, 0, c));
}

mat3 GetSplitMatrix(uint splitBit) {
    float b = float(splitBit);
    float c = 1.0f - b;
    // return mat3([ c, b, 0 ], [ 0.5, 0, 0.5 ], [ 0, c, b ]);
    // @TODO fix this later
    return transpose(mat3(
        c, b, 0.0,
        0.5, 0.0, 0.5,
        0, c, b));
}

uint getBit(uint nodeId, int bitId) {
    return (nodeId >> bitId) & 1u;
}

mat3 GetTransformationMatrix(uint nodeId, int depth) {
    int bitId = max(depth - 1, 0);
    mat3 transform = GetSquareMatrix(getBit(nodeId, bitId));
    for (int i = depth - 2; i >= 0; --i) {
        mat3 splitMatrix = GetSplitMatrix(getBit(nodeId, i));
        transform = splitMatrix * transform;
    }
    return transform;
}

uint leb_GetBitValue(uint bitField, uint bitID) {
    return (bitField >> bitID) & 1u;
}

uvec4 leb_SplitNodeIDs(uvec4 neighbours, uint b) {
    uint c = b ^ 1u;
    bool cb = bool(c);
    return uvec4(
        (neighbours[2 + b] << 1u) | uint(cb && bool(neighbours[2 + b])),
        (neighbours[2 + c] << 1u) | uint(cb && bool(neighbours[2 + c])),
        (neighbours[b] << 1u) | uint(cb && bool(neighbours[b])),
        (neighbours[3] << 1u) | b);
}

cbtNode leb_EdgeNeighbour(cbtNode node) {
    uint b = leb_GetBitValue(node.id, max(0, node.depth - 1));
    uvec4 neighbours = uvec4(0u, 0u, 3u - b, 2u + b);
    for (int bitID = int(node.depth) - 2; bitID >= 0; --bitID) {
        neighbours = leb_SplitNodeIDs(neighbours, leb_GetBitValue(node.id, bitID));
    }
    // Create edge node
    cbtNode edge;
    edge.id = neighbours[2];
    edge.depth = edge.id == 0u ? 0 : node.depth;
    return edge;
}

struct lebDiamondParent {
    cbtNode base;
    cbtNode top;
};

lebDiamondParent leb_DecodeDiamondParent(cbtNode node) {
    cbtNode parent = cbt_ParentNode(node);
    cbtNode edgeNeighbour = leb_EdgeNeighbour(parent);
    return lebDiamondParent(parent, edgeNeighbour);
}

bool leb_HasDiamondParent(lebDiamondParent diamondParent) {
    uint maxDepth = cbt_MaxDepth(heap[0]);
    bool canMergeBase = heapRead(diamondParent.base, maxDepth) <= 2u;
    bool canMergeTop = heapRead(diamondParent.top, maxDepth) <= 2u;
    return canMergeBase && canMergeTop;
}

#ifdef CBT_ENABLE_WRITE

void leb_MergeNodeSquare(const cbtNode node, const lebDiamondParent diamondParent) {
    if ((node.depth > 1) && leb_HasDiamondParent(diamondParent)) {
        cbt_MergeNode(node);
    }
}

void leb_SplitNodeSquare(cbtNode node) {
    if (!cbt_IsCeilNode(node)) {
        const uint minNodeID = 1u;
        cbtNode iterator = node;
        cbt_SplitNode(iterator);

        iterator = leb_EdgeNeighbour(iterator);

        while (iterator.id > minNodeID) {
            cbt_SplitNode(iterator);

            // Calculate parent node
            iterator = cbt_ParentNode(iterator);
            if (iterator.id > minNodeID) {
                cbt_SplitNode(iterator);
                iterator = leb_EdgeNeighbour(iterator);
            }
        }
    }
}
#endif

#endif