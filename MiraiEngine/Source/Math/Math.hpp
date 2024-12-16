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

        float distance_to_point(const glm::vec3 &p) const {
            return dot(p, normal) + distance;
        }

        Plane() {}

        glm::vec3 normal;
        float distance;
    };

    struct AABB {
        glm::vec3 min;
        glm::vec3 max;

        void translate(const glm::vec3 &translation) {
            min += translation;
            max += translation;
        }

        // https://x.com/Herschel/status/1188613724665335808/photo/2
        /**
         * Calculate the rotated minimum and maximum component for each axis
         * Add maximum value to max and minimum value to min
         */
        void transform(const glm::mat4 &transform) {
            glm::vec3 translation = transform[3];

            glm::vec3 vmin = translation;
            glm::vec3 vmax = translation;
            float a, b;
            for (int i = 0; i < 3; ++i) {
                for (int j = 0; j < 3; ++j) {
                    a = transform[j][i] * min[j];
                    b = transform[j][i] * max[j];

                    if (a < b) {
                        vmin[i] += a;
                        vmax[i] += b;
                    } else {
                        vmin[i] += b;
                        vmax[i] += a;
                    }
                }
            }

            min = vmin;
            max = vmax;
        }
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

        void create_from_matrix(const glm::mat4 &m, const glm::mat4 &inv_m);

        bool intersect(const AABB &aabb) const;

        std::array<Plane, 6> planes;
        std::array<glm::vec3, 8> points;
    };
} // namespace mirai