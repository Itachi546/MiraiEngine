#ifndef CUBEMAP_GLSL
#define CUBEMAP_GLSL
/*
// Cubemap helper function for GLSL
vec3 uv_to_xyz(ivec3 cubeCoord, vec2 cubemapSize) {
    vec2 uv = vec2(cubeCoord.xy) / cubemapSize;
    uv = uv * 2.0f - 1.0f;

    switch (cubeCoord.z) {
    case 0: return vec3(-uv.x, uv.y, -1.0f); // +X
    case 1: return vec3(uv.x, uv.y, 1.0f);   // -X
    case 2: return vec3(uv.y, -1.0f, -uv.x); // +Y
    case 3: return vec3(-uv.y, 1.0f, -uv.x); // -Y
    case 4: return vec3(1.0f, uv.y, -uv.x);  // +Z
    case 5: return vec3(-1.0f, uv.y, uv.x);  // -Z
    }
    return vec3(0.0);
}
*/
vec3 uv_to_xyz(ivec3 cubeCoord, vec2 cubemapSize) {
    vec2 uv = vec2(cubeCoord.xy) / cubemapSize;
    uv = uv * 2.0f - 1.0f;
    int face = cubeCoord.z;
    if (face == 0)
        return vec3(1.f, uv.y, -uv.x);

    else if (face == 1)
        return vec3(-1.f, uv.y, uv.x);

    else if (face == 2)
        return vec3(+uv.x, -1.f, +uv.y);

    else if (face == 3)
        return vec3(+uv.x, 1.f, -uv.y);

    else if (face == 4)
        return vec3(+uv.x, uv.y, 1.f);

    else { // if(face == 5)
        return vec3(-uv.x, +uv.y, -1.f);
    }
}

#endif