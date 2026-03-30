#ifndef CUBEMAP_UTILS_HLSLI
#define CUBEMAP_UTILS_HLSLI

float3 uv_to_xyz(int3 coord, float2 size) {
    float2 uv  = float2(coord.xy) / size;
    uv = uv * 2.0f - 1.0f;
    int face = coord.z;
    if (face == 0)
        return float3(1.f, uv.y, -uv.x);

    else if (face == 1)
        return float3(-1.f, uv.y, uv.x);

    else if (face == 2)
        return float3(+uv.x, -1.f, +uv.y);

    else if (face == 3)
        return float3(+uv.x, 1.f, -uv.y);

    else if (face == 4)
        return float3(+uv.x, uv.y, 1.f);

    else { // if(face == 5)
        return float3(-uv.x, +uv.y, -1.f);
    }
}

#endif