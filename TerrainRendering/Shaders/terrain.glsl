#ifndef TERRAIN_GLSL
#define TERRAIN_GLSL

#include "cbt.glsl"

const mat2x3 faceVertices = mat2x3(vec3(0, 0, 1), vec3(1, 0, 0));

#define WORLD_SIZE 32000.0f
#define HEIGHT_SCALE 2400.0f

vec2 get_uv(vec2 position) {
    vec2 world_size = vec2(WORLD_SIZE);
    return (position / world_size) * 0.5 + 0.5;
}

float get_height(vec2 position) {
    vec2 uv = get_uv(position);
    return (texture(uHeightmap, uv).r * 2.0f - 1.0f) * HEIGHT_SCALE;
}

vec3 get_normal(vec2 p) {
    vec2 texCoord = get_uv(p);
    float filterSize = 1.0f / float(textureSize(uHeightmap, 0).x); // sqrt(dot(dFdx(texCoord), dFdy(texCoord)));
    float sx0 = textureLod(uHeightmap, texCoord - vec2(filterSize, 0.0), 0.0).r;
    float sx1 = textureLod(uHeightmap, texCoord + vec2(filterSize, 0.0), 0.0).r;
    float sy0 = textureLod(uHeightmap, texCoord - vec2(0.0, filterSize), 0.0).r;
    float sy1 = textureLod(uHeightmap, texCoord + vec2(0.0, filterSize), 0.0).r;
    float sx = sx1 - sx0;
    float sy = sy1 - sy0;

    return normalize(vec3(0.03 / filterSize * 0.5f * vec2(-sx, -sy), 1));
}

vec4 toWorldPos(vec4 p) {
    p.xz = 2.0f * p.xz - 1.0f;
    p.xz *= WORLD_SIZE;
    p.y += get_height(p.xz);
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