#pragma once

#define GLM_FORCE_XYZW_ONLY
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <array>

namespace mirai {

    struct Plane {
        Plane(const glm::vec3 &normal, float distance) : normal(normal), distance(distance) {
        }

        Plane(const glm::vec4 &plane) : normal(plane), distance(plane.w) {
        }

        void create_from(const glm::vec3 &normal, float distance) {
            this->normal = normal;
            this->distance = distance;
        }

        void create_from(const glm::vec4 &plane) {
            this->normal = glm::vec3(plane);
            this->distance = plane.w;
        }

        Plane() {}

        glm::vec3 normal;
        float distance;
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;
    };

    struct Frustum {

        enum FRUSTUM_PLANE {
            FRUSTUM_PLANE_LEFT = 0,
            FRUSTUM_PLANE_RIGHT,
            FRUSTUM_PLANE_TOP,
            FRUSTUM_PLANE_BOTTOM,
            FRUSTUM_PLANE_NEAR,
            FRUSTUM_PLANE_FAR
        };

        void create_from_matrix(const glm::mat4 &m);

        std::array<Plane, 6> planes;
    };
} // namespace mirai