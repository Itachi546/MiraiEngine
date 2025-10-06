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
    mat4 VP;
    vec4 frustumPlanes[6];
    // xyz consists of camera position, w consists of split/merge flag
    vec4 subdivisionInfo;
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

float TriangleLevelOfDetail(in const vec4[3] patchVertices) {
    vec3 v0 = (VP * patchVertices[0]).xyz;
    vec3 v2 = (VP * patchVertices[2]).xyz;

    vec3 edgeCenter = (v0 + v2);
    vec3 edgeVector = v2 - v0;
    float distanceToEdgeSqr = dot(edgeCenter, edgeCenter);
    float edgeLengthSqr = dot(edgeVector, edgeVector);

    float lodFactor = subdivisionInfo.y;
    return lodFactor + log2(edgeLengthSqr / distanceToEdgeSqr);
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
    return vec2(TriangleLevelOfDetail(patchVertices), 1.0f);
}

void main() {
    uint id = gl_GlobalInvocationID.x;
    if (id < leafCount) {
        cbtNode node = cbt_BinarySearch(id);
        
        vec4[3] faceVertices = DecodeTriangleVertices(node);

        float mode = subdivisionInfo.x;
        if (mode > 0.5f) {
            // Split
            vec2 targetLod = LevelOfDetail(faceVertices);
            if (targetLod.x > 1.0f) {
                leb_SplitNodeSquare(node);
            }
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