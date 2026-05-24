#ifndef RAYCAST_GLSL
#define RAYCAST_GLSL

vec3 generate_camera_ray(vec2 ndc, mat4 invP, mat4 invV) {
    vec3 clipPos;
    clipPos.x = invP[0][0] * ndc.x + invP[0][1] * ndc.y - invP[0][2];
    clipPos.y = invP[1][0] * ndc.x + invP[1][1] * ndc.y - invP[1][2];
    clipPos.z = -1.0f;

    vec3 worldPos = mat3(invV) * clipPos;
    return normalize(worldPos.xyz);
}

#endif