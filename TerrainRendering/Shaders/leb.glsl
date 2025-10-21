#ifndef LEB_GLSL
#define LEB_GLSL

mat3 GetSquareMatrix(uint splitBit) {
    float b = float(splitBit);
    float c = 1.0f - b;
    return mat3(c, b, b, 0, c, 0, b, b, c);
}

mat3 GetSplitMatrix(uint splitBit) {
    float b = float(splitBit);
    float c = 1.0f - b;
    return mat3(c, 0.5f, 0, b, 0.0f, c, 0.0f, 0.5f, b);
}

mat3 GetWindingMatrix(uint mirrorBit) {
    float b = float(mirrorBit);
    float c = 1.0f - b;
    return mat3(c, 0.0f, b, 0.0f, 1.0f, 0.0f, b, 0.0f, c);
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
    return GetWindingMatrix(depth & 1) * transform;
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

uvec4 leb_DecodeSameDepthNeighbourIDs(cbt_Node node) {
    uint b = leb_GetBitValue(node.id, max(0, node.depth - 1));
    // left, right, edge, node
    uvec4 neighbours = uvec4(0u, 0u, 3u - b, 2u + b);
    for (int bitID = node.depth - 2; bitID >= 0; --bitID) {
        neighbours = leb_SplitNodeIDs(neighbours, leb_GetBitValue(node.id, bitID));
    }
    return neighbours;
}

struct lebDiamondParent {
    cbt_Node base;
    cbt_Node top;
};

lebDiamondParent leb_DecodeDiamondParent(cbt_Node node) {
    cbt_Node parent = cbt_ParentNode(node);
    uvec4 neighbours = leb_DecodeSameDepthNeighbourIDs(parent);
    cbt_Node edgeNeighbour;
    edgeNeighbour.id = neighbours[2] > 0u ? neighbours[2] : parent.id;
    edgeNeighbour.depth = parent.depth;
    return lebDiamondParent(parent, edgeNeighbour);
}

bool leb_HasDiamondParent(lebDiamondParent diamondParent, bool enable_sum_reduction_prepass) {
    int maxDepth = cbt_MaxDepth();
    bool canMergeBase = heapRead(diamondParent.base, maxDepth, enable_sum_reduction_prepass) <= 2u;
    bool canMergeTop = heapRead(diamondParent.top, maxDepth, enable_sum_reduction_prepass) <= 2u;
    return canMergeBase && canMergeTop;
}

#ifdef CBT_ENABLE_WRITE

void leb_MergeNodeSquare(const cbt_Node node, const lebDiamondParent diamondParent, bool enable_sum_reduction_prepass) {
    if ((node.depth > 1) && leb_HasDiamondParent(diamondParent, enable_sum_reduction_prepass)) {
        cbt_MergeNode(node);
    }
}

cbt_Node leb_EdgeNeighbour(cbt_Node node) {
    uvec4 neighbours = leb_DecodeSameDepthNeighbourIDs(node);
    return cbt_CreateNode(
        neighbours[2],
        neighbours[2] == 0u ? 0 : node.depth);
}

void leb_SplitNodeSquare(cbt_Node node) {
    if (!cbt_IsCeilNode(node)) {
        const uint minNodeID = 1u;
        cbt_Node iterator = node;
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