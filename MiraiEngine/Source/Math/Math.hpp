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

        void translate(const glm::vec3 &translation) {
            min += translation;
            max += translation;
        }

        void transform(const glm::mat4 &transform) {
            glm::vec3 vmin{FLT_MAX};
            glm::vec3 vmax{-FLT_MAX};

            glm::vec3 corners[] = {
                glm::vec3(min.x, min.y, min.z),
                glm::vec3(min.x, max.y, min.z),
                glm::vec3(min.x, min.y, max.z),
                glm::vec3(min.x, max.y, max.z),
                glm::vec3(max.x, min.y, min.z),
                glm::vec3(max.x, max.y, min.z),
                glm::vec3(max.x, min.y, max.z),
                glm::vec3(max.x, max.y, max.z),
            };

            for (auto &v : corners) {
                v = transform * glm::vec4(v, 1.0f);
                vmin = glm::min(v, vmin);
                vmax = glm::max(v, vmax);
            }

            min = vmin;
            max = vmax;
        }
        // https://x.com/Herschel/status/1188613724665335808/photo/2
        void transform_fast(const glm::mat4 &transform) {
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

        void create_from_matrix(const glm::mat4 &m);

        std::array<Plane, 6> planes;
    };
} // namespace mirai