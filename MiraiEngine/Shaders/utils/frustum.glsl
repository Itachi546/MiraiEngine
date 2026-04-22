#ifndef FRUSTUM_GLSL
#define FRUSTUM_GLSL

struct TileFrustum {
    // Left, Right, Top, Bottom
    vec4 planes[4];
};

bool point_inside_plane(vec4 plane, vec3 p) {
    return dot(plane.xyz, p) + plane.w >= 0;
}

bool sphere_inside_plane(vec4 sphere, vec4 plane) {
    return dot(plane.xyz, sphere.xyz) + plane.w >= -sphere.w;
}

bool sphere_inside_frustum(TileFrustum frustum, vec4 sphere, float znear, float zfar) {
    // Z-coordinate is all negative, so we have to invert sign when comparing
    // check if the sphere lies outside zfar
    if (sphere.z - sphere.w > znear)
        return false;

    if (sphere.z + sphere.w < zfar)
        return false;

    for (int i = 0; i < 4; ++i) {
        if (!sphere_inside_plane(sphere, frustum.planes[i])) {
            return false;
        }
    }
    return true;
}

bool cone_inside_plane(vec4 plane, vec3 position, vec3 direction, vec3 base, float radius) {
    vec3 m = cross(cross(plane.xyz, direction), direction);
    vec3 q = base - radius * m;

    return point_inside_plane(plane, position) || point_inside_plane(plane, q);
}

bool cone_inside_frustum(TileFrustum frustum, vec3 position, vec3 direction, float height, float radius, float znear, float zfar) {
    vec3 base = position + direction * height;
    vec4 near_plane = vec4(0.0f, 0.0f, -1.0f, znear);
    vec4 far_plane = vec4(0.0f, 0.0f, 1.0f, -zfar);

    if (!cone_inside_plane(near_plane, position, direction, base, radius))
        return false;
    if (!cone_inside_plane(far_plane, position, direction, base, radius))
        return false;
    
    for (int i = 0; i < 4; ++i) {
        if (!cone_inside_plane(frustum.planes[i], position, direction, base, radius))
            return false;
    }
    return true;
}

#endif