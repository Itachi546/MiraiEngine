#version 460
#extension GL_GOOGLE_include_directive : enable

layout(local_size_x = 256, local_size_y = 1, local_size_z = 1) in;

layout(set = 0, binding = 0) buffer cbtTree {
    uint heap[];
};

layout(set = 0, binding = 1) buffer cbtDrawIndirect {
    uint leafCount;
};

layout(set = 0, binding = 2) uniform sampler2D uHeightmap;

#define CBT_ENABLE_WRITE
#include "leb.glsl"
#include "terrain.glsl"

layout(push_constant) uniform PushConstants {
    vec4 frustumPlanes[6];
    // xyz consists of camera position, w consists of split/merge flag
    vec4 cameraPosition;
};

bool IntersectFrustum(vec3 bmin, vec3 bmax) {
    for (int i = 0; i < 6; ++i) {
        vec3 n = frustumPlanes[i].xyz;
        bvec3 b = greaterThan(n, vec3(0.0));
        vec3 p = mix(bmin, bmax, b);

        float distanceToPlane = dot(vec4(p, 1.0f), frustumPlanes[i]);
        if (distanceToPlane < 0.0f)
            return false;
    }
    return true;
}

/*
Compute the level of detail of associated triangle
*/
vec2 LevelOfDetail(in const vec4[3] patchVertices) {

    vec3 bmin = min(min(patchVertices[0].xyz, patchVertices[1].xyz), patchVertices[2].xyz);
    vec3 bmax = max(max(patchVertices[0].xyz, patchVertices[1].xyz), patchVertices[2].xyz);

    if (!IntersectFrustum(bmin, bmax)) {
        return vec2(0.0f);
    }
    return vec2(1.0f);
}

/*
float Wedge(vec2 a, vec2 b) {
    return a.x * b.y - a.y * b.x;
}

bool IsInside(mat2x3 faceVertices) {
    vec2 v1 = vec2(faceVertices[0][0], faceVertices[1][0]);
    vec2 v2 = vec2(faceVertices[0][1], faceVertices[1][1]);
    vec2 v3 = vec2(faceVertices[0][2], faceVertices[1][2]);
    float w1 = Wedge(v2 - v1, uTargetPosition - v1);
    float w2 = Wedge(v3 - v2, uTargetPosition - v2);
    float w3 = Wedge(v1 - v3, uTargetPosition - v3);
    vec3 w = vec3(w1, w2, w3);
    bvec3 wb = greaterThanEqual(w, vec3(0.0f));
    return all(wb);
}
*/
void main() {
    uint id = gl_GlobalInvocationID.x;
    uint totalLeaves = cbt_HeapRead(cbtNode(1, 0));
    if (id < leafCount && leafCount == totalLeaves) {
        cbtNode node = cbt_BinarySearch(id);
        vec4[3] faceVertices = DecodeTriangleVertices(node);

        float mode = cameraPosition.w;
        if (mode > 0.5f) {
            // Split

            vec3 c = vec3(0.0f);
            for (int i = 0; i < 3; ++i)
                c += faceVertices[i].xyz;
            c /= 3.0f;

            float dist = length(cameraPosition.xyz - c);
            // vec2 targetLod = LevelOfDetail(faceVertices);
            /*
            if (targetLod.x > 0.5f) {
                leb_SplitNodeSquare(node);
            }
            */
            if (dist < 5000.0f)
                leb_SplitNodeSquare(node);
        } else {
            // Merge
            lebDiamondParent diamondParent = leb_DecodeDiamondParent(node);
            vec4[3] baseFaceVertices = DecodeTriangleVertices(diamondParent.base);
            bool shouldMergeBase = LevelOfDetail(baseFaceVertices).x < 0.5f;

            vec4[3] topFaceVertices = DecodeTriangleVertices(diamondParent.top);
            bool shouldMergeTop = LevelOfDetail(topFaceVertices).x < 0.5f;

            if (shouldMergeBase && shouldMergeBase) {
                leb_MergeNodeSquare(node, diamondParent);
            }
        }
    }
}