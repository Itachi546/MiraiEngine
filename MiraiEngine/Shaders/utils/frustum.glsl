#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

struct TileFrustum {
    // Left, Right, Top, Bottom
    vec4 planes[4];
};

bool sphere_inside_plane(vec4 sphere, vec4 plane) {
    return dot(plane.xyz, sphere.xyz) + plane.w >= -sphere.w;
}

bool sphere_inside_frustum(TileFrustum frustum, vec4 sphere, float znear, float zfar) {
    bool inside = true;
    if (sphere.z - sphere.r > znear || sphere.z + sphere.r < zfar)
        return false;

    for (int i = 0; i < 4; ++i) {
        if (!sphere_inside_plane(sphere, frustum.planes[i])) {
            return false;
        }
    }
    return inside;
}

#endif