#ifndef DDGI_GLSL
#define DDGI_GLSL

#include "../utils/math.glsl"

/**  Generate a spherical fibonacci point

    http://lgdv.cs.fau.de/publications/publication/Pub.2015.tech.IMMD.IMMD9.spheri/

    To generate a nearly uniform point distribution on the unit sphere of size N, do
    for (float i = 0.0; i < N; i += 1.0) {
        float3 point = sphericalFibonacci(i,N);
    }

    The points go from z = +1 down to z = -1 in a spiral. To generate samples on the +z hemisphere,
    just stop before i > N/2.

*/
vec3 spherical_fibonacci(float i, float n) {
    const float PHI = sqrt(5) * 0.5 + 0.5;
#define madfrac(A, B) ((A) * (B) - floor((A) * (B)))
    float phi = 2.0 * PI * madfrac(i, PHI - 1);
    float cosTheta = 1.0 - (2.0 * i + 1.0) * (1.0 / n);
    float sinTheta = sqrt(clamp(1.0 - cosTheta * cosTheta, 0.0, 1.0));

    return vec3(
        cos(phi) * sinTheta,
        sin(phi) * sinTheta,
        cosTheta);

#undef madfrac
}

vec3 grid_coord_to_position(vec3 grid_coord) {
    return probe_step * grid_coord + probe_start_position;
}

ivec3 probe_index_to_grid_coord(int index) {
    ivec3 ipos;
    ipos.x = index & (probe_counts.x - 1);
    ipos.y = (index & ((probe_counts.x * probe_counts.y) - 1)) >> findMSB(probe_counts.x);
    ipos.z = index >> findMSB(probe_counts.x * probe_counts.y);
    return ipos;
}

vec3 probe_location(int index) {
    return grid_coord_to_position(probe_index_to_grid_coord(index));
}

#endif