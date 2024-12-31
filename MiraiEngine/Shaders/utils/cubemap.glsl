#ifndef CUBEMAP_GLSL
#define CUBEMAP_GLSL

// Cubemap helper function for GLSL
vec3 uv_to_xyz(ivec3 cubeCoord, vec2 cubemapSize) {
    vec2 texCoord = vec2(cubeCoord.xy) / cubemapSize;
    texCoord = texCoord * 2.0f - 1.0f;
    switch (cubeCoord.z) {
    case 0: return vec3(1.0f, -texCoord.yx);             // +X
    case 1: return vec3(-1.0f, -texCoord.y, texCoord.x); // -X
    case 2: return vec3(texCoord.x, 1.0f, texCoord.y);   // +Y
    case 3: return vec3(texCoord.x, -1.0f, -texCoord.y); // -Y
    case 4: return vec3(texCoord.x, -texCoord.y, 1.0f);  // +Z
    case 5: return vec3(-texCoord.xy, -1.0f);            // -Z
    }
    return vec3(0.0);
}


#endif