#ifndef TERRAIN_GLSL
#define TERRAIN_GLSL

#include "cbt.glsl"

const mat2x3 faceVertices = mat2x3(vec3(0, 0, 1), vec3(1, 0, 0));

vec4 toWorldPos(vec4 p) {
    p.xz -= 0.5f;
    p.xz *= 2048.0f;
    return p;
}

vec4[3] DecodeTriangleVertices(const cbtNode node) {
    mat3 transformMatrix = GetTransformationMatrix(node.id, int(node.depth));
    mat2x3 faceVertices = transformMatrix * faceVertices;
    vec4 v1 = vec4(faceVertices[0][0], 0.0f, faceVertices[1][0], 1.0f);
    vec4 v2 = vec4(faceVertices[0][1], 0.0f, faceVertices[1][1], 1.0f);
    vec4 v3 = vec4(faceVertices[0][2], 0.0f, faceVertices[1][2], 1.0f);

    return vec4[3](toWorldPos(v1), toWorldPos(v2), toWorldPos(v3));
}

#endif