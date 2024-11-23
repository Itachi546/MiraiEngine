#include "Math.hpp"

void mirai::Frustum::create_from_matrix(const glm::mat4 &m) {

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
}