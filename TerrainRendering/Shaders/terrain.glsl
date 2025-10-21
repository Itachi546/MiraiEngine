#ifndef TERRAIN_GLSL
#define TERRAIN_GLSL

const mat2x3 faceVertices = mat2x3(vec3(0, 0, 1), vec3(1, 0, 0));

vec2 get_uv(vec2 position, vec2 world_size) {
    return (position / world_size) * 0.5 + 0.5;
}

float get_height(vec2 position, vec2 world_size, float max_height) {
    vec2 uv = get_uv(position, world_size);
    return (texture(uHeightmap, uv).r * 2.0f - 1.0f) * max_height;
}

vec3 get_normal(vec2 p, vec2 world_size) {
    vec2 texCoord = get_uv(p, world_size);
    float filterSize = 1.0f / float(textureSize(uHeightmap, 0).x); // sqrt(dot(dFdx(texCoord), dFdy(texCoord)));
    float sx0 = textureLod(uHeightmap, texCoord - vec2(filterSize, 0.0), 0.0).r;
    float sx1 = textureLod(uHeightmap, texCoord + vec2(filterSize, 0.0), 0.0).r;
    float sy0 = textureLod(uHeightmap, texCoord - vec2(0.0, filterSize), 0.0).r;
    float sy1 = textureLod(uHeightmap, texCoord + vec2(0.0, filterSize), 0.0).r;
    float sx = sx1 - sx0;
    float sy = sy1 - sy0;

    return normalize(vec3(0.03 / filterSize * 0.5f * vec2(-sx, -sy), 1));
}

vec4 toWorldPos(vec4 p, vec2 world_size, float max_height) {
    p.xz = 2.0f * p.xz - 1.0f;
    p.xz *= world_size;
    #ifdef FLAG_DISPLACE
    p.y += get_height(p.xz, world_size, max_height);
    #endif
    return p;
}

vec4[3] DecodeTriangleVertices(const cbt_Node node, vec2 world_size, float max_height) {
    mat3 transformMatrix = GetTransformationMatrix(node.id, node.depth);
    mat2x3 faceVertices = transformMatrix * faceVertices;
    vec4 v1 = vec4(faceVertices[0][0], 0.0f, faceVertices[1][0], 1.0f);
    vec4 v2 = vec4(faceVertices[0][1], 0.0f, faceVertices[1][1], 1.0f);
    vec4 v3 = vec4(faceVertices[0][2], 0.0f, faceVertices[1][2], 1.0f);

    return vec4[3](toWorldPos(v1, world_size, max_height), toWorldPos(v2, world_size, max_height), toWorldPos(v3, world_size, max_height));
}

#endif