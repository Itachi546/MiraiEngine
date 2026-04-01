#ifndef TRANSFORM_HLSLI
#define TRANSFORM_HLSLI

float linearize_depth(float d, float near, float far) {
    return (near * far) / (far - d * (far - near));
}

#endif

