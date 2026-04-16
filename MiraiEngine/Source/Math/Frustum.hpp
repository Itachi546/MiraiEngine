#ifndef FRUSTUM_HPP
#define FRUSTUM_HPP
#include "Math.hpp"

namespace mirai {

    struct FrustumPlanes {

        enum FRUSTUM_PLANE {
            FRUSTUM_PLANE_LEFT = 0,
            FRUSTUM_PLANE_RIGHT,
            FRUSTUM_PLANE_TOP,
            FRUSTUM_PLANE_BOTTOM,
            FRUSTUM_PLANE_NEAR,
            FRUSTUM_PLANE_FAR
        };

        void create_from_matrix(const glm::mat4 &m);

        bool intersect(const AABB &aabb) const;

        bool intersect(glm::vec3 position, float radius) const;

        std::array<Plane, 6> planes;
    };

    struct FrustumPoints {

        enum FRUSTUM_POINT {
            NTL = 0,
            NTR,
            NBR,
            NBL,
            FTL,
            FTR,
            FBR,
            FBL,
        };

        static void calculate_frustum_corners(const glm::mat4 &inv_m, std::array<glm::vec3, 8> &corners);

        void create_from_matrix(const glm::mat4 &inv_m);

        std::array<glm::vec3, 8> points;
    };
} // namespace mirai

#endif