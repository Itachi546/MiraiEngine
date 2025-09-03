#ifndef LEB_GLSL
#define LEB_GLSL

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

#endif