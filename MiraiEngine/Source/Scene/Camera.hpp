#pragma once

#include "Math/Math.hpp"

namespace mirai {
    enum ProjectionMode {
        PROJECTION_MODE_ORTHOGRAPHIC,
        PROJECTION_MODE_PERSPECTIVE
    };

    class Camera {
      public:
        Camera();

        void update();

        glm::mat4 get_view_transform() const {
            return view_matrix;
        }

        glm::mat4 get_projection_transform() const {
            return projection_matrix;
        }

        glm::mat4 get_view_projection_transform() const {
            return view_projection_matrix;
        }

        glm::mat4 get_inv_projection_transform() const {
            return inv_projection_matrix;
        }

        glm::mat4 get_inv_view_transform() const {
            return inv_view_matrix;
        }

        void set_projection_mode(ProjectionMode projection_mode) {
            this->projection_mode = projection_mode;
        }

        void set_fov(float fov) {
            this->fov = fov;
        }

        void set_near_plane(float near_plane) {
            this->near_plane = near_plane;
        }

        void set_far_plane(float far_plane) {
            this->far_plane = far_plane;
        }

        float get_fov() const {
            return fov;
        }

        float get_aspect_ratio() const {
            return aspect_ratio;
        }

        float get_near_plane() const {
            return near_plane;
        }

        float get_far_plane() const {
            return far_plane;
        }

        glm::vec3 get_right() const {
            return right;
        }

        glm::vec3 get_up() const {
            return up;
        }

        glm::vec3 get_forward() const {
            return forward;
        }

        Frustum &get_frustum() {
            return frustum;
        }

        glm::vec3 position;
        glm::vec3 rotation;

      private:
        glm::mat4 view_matrix, inv_view_matrix;
        glm::mat4 projection_matrix, inv_projection_matrix;
        glm::mat4 viewport_matrix;
        glm::mat4 view_projection_matrix;

        ProjectionMode projection_mode;

        glm::vec3 forward, right, up;

        float fov;
        float aspect_ratio;
        float near_plane;
        float far_plane;

        Frustum frustum;

        void update_projection_matrix();
    };
} // namespace mirai