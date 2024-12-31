#ifndef DIRECTIONAL_SHADOW_GLSL
#define DIRECTIONAL_SHADOW_GLSL

vec2 poissonDisk[16] = vec2[](
    vec2(-0.94201624, -0.39906216),
    vec2(0.94558609, -0.76890725),
    vec2(-0.094184101, -0.92938870),
    vec2(0.34495938, 0.29387760),
    vec2(-0.91588581, 0.45771432),
    vec2(-0.81544232, -0.87912464),
    vec2(-0.38277543, 0.27676845),
    vec2(0.97484398, 0.75648379),
    vec2(0.44323325, -0.97511554),
    vec2(0.53742981, -0.47373420),
    vec2(-0.26496911, -0.41893023),
    vec2(0.79197514, 0.19090188),
    vec2(-0.24188840, 0.99706507),
    vec2(-0.81409955, 0.91437590),
    vec2(0.19984126, 0.78641367),
    vec2(0.14383161, -0.14100790));

float rand(vec2 uv) {
    float dot_product = dot(uv, vec2(12.9898, 78.233));
    return fract(sin(dot_product) * 43758.5453);
}

float texture_proj(vec4 shadowCoord, vec2 offset, int cascadeIndex, float bias) {
    float shadow = 1.0;
    float currentDepth = shadowCoord.z;
    if (currentDepth > -1.0 && currentDepth < 1.0) {
        float depthFromTexture = texture(shadow_depth_texture, vec3(shadowCoord.xy + offset, cascadeIndex)).r;
        if (shadowCoord.w > 0.0 && depthFromTexture < currentDepth)
            shadow = 0.0f;
    }
    return shadow;
}

float calculate_shadow_from_texture(vec3 worldPos, int cascadeIndex) {
    if (cascadeIndex >= NUM_DIRLIGHT_CASCADE)
        return 1.0f;

    mat4 cascadeVP = cascade_info.VP[cascadeIndex];
    // Transform into light NDC Coordinate
    vec4 shadowCoord = cascadeVP * vec4(worldPos, 1.0f);
    shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;

    float shadowFactor = 0.0f;
    const float scale = 1.0f;
    vec2 shadowDims = vec2(cascade_info.dims[1], cascade_info.dims[2]);
    vec2 invRes = scale / shadowDims.xy;

    int kSampleRadius = 2;
    int sampleCount = 0;

    const int kPCFRadiusMultiplier = 1;
    float multiplierX = invRes.x * kPCFRadiusMultiplier;
    float multiplierY = invRes.y * kPCFRadiusMultiplier;

    for (int x = -kSampleRadius; x <= kSampleRadius; ++x) {
        for (int y = -kSampleRadius; y <= kSampleRadius; ++y) {
            vec2 coord = vec2(x * multiplierX, y * multiplierY);
            int index = int(rand(coord) * 15.0);
            shadowFactor += texture_proj(shadowCoord / shadowCoord.w, coord + poissonDisk[index] * invRes, cascadeIndex, 0.001f);
            sampleCount++;
        }
    }
    return shadowFactor / sampleCount;
}

float calculate_shadow_factor(vec3 worldPos, float camDist, out int cascadeIndex) {

    cascadeIndex = -1;
    float zRange = cascade_info.dims[0];
    for (int i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
        if (camDist <= cascade_info.split_distances[i] * zRange) {
            cascadeIndex = i;
            break;
        }
    }
    if (cascadeIndex == -1)
        return 1.0f;
    return calculate_shadow_from_texture(worldPos, cascadeIndex);
}

#endif