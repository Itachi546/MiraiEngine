#include "Math.hpp"

const glm::vec3 frustum_corner_ndc[8] = {
    glm::vec3(-1.0f, 1.0f, -1.0f),  // NTL
    glm::vec3(1.0f, 1.0f, -1.0f),   // NTR
    glm::vec3(1.0f, -1.0f, -1.0f),  // NBR
    glm::vec3(-1.0f, -1.0f, -1.0f), // NBL
    glm::vec3(-1.0f, 1.0f, 1.0f),   // FTL
    glm::vec3(1.0f, 1.0f, 1.0f),    // FTR
    glm::vec3(1.0f, -1.0f, 1.0f),   // FBR
    glm::vec3(-1.0f, -1.0f, 1.0f),  // FBL
};

void mirai::Frustum::calculate_frustum_corners(const glm::mat4 &inv_m, std::array<glm::vec3, 8> &corners) {
    for (int i = 0; i < 8; ++i) {
        glm::vec4 corner = inv_m * glm::vec4(frustum_corner_ndc[i], 1.0f);
        corners[i] = corner / corner.w;
    }
}

void mirai::Frustum::create_from_matrix(const glm::mat4 &m, const glm::mat4 &inv_m) {

    glm::vec4 x = glm::vec4{m[0][0], m[1][0], m[2][0], m[3][0]};
    glm::vec4 y = glm::vec4{m[0][1], m[1][1], m[2][1], m[3][1]};
    glm::vec4 z = glm::vec4{m[0][2], m[1][2], m[2][2], m[3][2]};
    glm::vec4 w = glm::vec4{m[0][3], m[1][3], m[2][3], m[3][3]};

    planes[FRUSTUM_PLANE_LEFT].create_from(w + x);
    planes[FRUSTUM_PLANE_RIGHT].create_from(w - x);
    planes[FRUSTUM_PLANE_TOP].create_from(w + y);
    planes[FRUSTUM_PLANE_BOTTOM].create_from(w - y);
    planes[FRUSTUM_PLANE_NEAR].create_from(w + z);
    planes[FRUSTUM_PLANE_FAR].create_from(w - z);

    for (int i = 0; i < 8; ++i) {
        glm::vec4 corner = inv_m * glm::vec4(frustum_corner_ndc[i], 1.0f);
        points[i] = corner / corner.w;
    }
}

bool mirai::Frustum::intersect(const AABB &aabb, bool skip_near_plane) const {
    const glm::vec3 &min = aabb.min;
    const glm::vec3 &max = aabb.max;

    for (int i = 0; i < 6; ++i) {
        if (skip_near_plane && i == FRUSTUM_PLANE_NEAR)
            continue;

        glm::vec3 p = min;
        const glm::vec3 &normal = planes[i].normal;
        if (normal.x >= 0.0f)
            p.x = max.x;
        if (normal.y >= 0.0f)
            p.y = max.y;
        if (normal.z >= 0.0f)
            p.z = max.z;

        if (planes[i].distance_to_point(p) < 0.0f)
            return false;
    }
    return true;
}
