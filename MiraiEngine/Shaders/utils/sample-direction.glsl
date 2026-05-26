#ifndef SAMPLE_DIRECTION_GLSL
#define SAMPLE_DIRECTION_GLSL

#include "../utils/math.glsl"
// Utility function to get a vector perpendicular to an input vector
// (from "Efficient Construction of Perpendicular Vectors Without Branching")
vec3 get_perpendicular_vector(vec3 u) {
    vec3 a = abs(u);
    uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
    uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
    uint zm = 1 ^ (xm | ym);
    return cross(u, vec3(xm, ym, zm));
}

// Cosine-weighted hemisphere sample around normal n
vec3 cosine_hemisphere_sample(vec3 n, vec2 xi) {
    float phi = 2.0 * PI * xi.x;
    float cos_theta = sqrt(1.0 - xi.y);
    float sin_theta = sqrt(xi.y);

    // Build ONB around n
    vec3 up = abs(n.z) < 0.999 ? vec3(0, 0, 1) : vec3(1, 0, 0);
    vec3 tx = normalize(cross(up, n));
    vec3 ty = cross(n, tx);

    return normalize(tx * (sin_theta * cos(phi)) +
                     ty * (sin_theta * sin(phi)) +
                     n * cos_theta);
}

vec3 get_cone_sample(vec2 rand, vec3 light_dir, float cos_theta_max) {

    vec3 bitangent = get_perpendicular_vector(light_dir);
    vec3 tangent = cross(bitangent, light_dir);

    float cos_theta = mix(cos_theta_max, 1.0, rand.x);

    float sin_theta = sqrt(1.0 - cos_theta * cos_theta);

    float phi = rand.y * 2.0 * PI;

    return tangent * (sin_theta * cos(phi)) + bitangent * (sin_theta * sin(phi)) + light_dir * cos_theta;
}

#endif