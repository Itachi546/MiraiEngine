#include "Camera.hpp"

#include "Device/Window.hpp"
#include "Math/Angles.hpp"

namespace mirai {
    Camera::Camera() : position(glm::vec3(0.0f, 0.0f, -3.0f)),
                       rotation(glm::vec3(0.0f)),
                       fov(60.0f),
                       aspect_ratio(4.0f / 3.0f),
                       near_plane(0.2f),
                       far_plane(100.0f),
                       jitter_factor(glm::vec2(0.0f)),
                       projection_mode(PROJECTION_MODE_PERSPECTIVE) {
        view_projection_matrix = glm::mat4(1.0f);
    }

    void Camera::update() {
        glm::vec3 rotation_radians = glm::radians(rotation);
        forward = rotation_to_direction(rotation_radians);
        right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        up = glm::normalize(glm::cross(right, forward));

        view_matrix = glm::lookAt(position, position + forward, up);

        inv_view_matrix = glm::inverse(view_matrix);

        update_projection_matrix();

        view_projection_matrix = projection_matrix * view_matrix;
        inv_view_projection_matrix = glm::inverse(view_projection_matrix);

        frustum_planes.create_from_matrix(view_projection_matrix);
        /*
        glm::vec3 near_point = position - near_plane * forward;

        float tan_fov = tan(glm::radians(fov * 0.5f));
        float hH = tan_fov * near_plane;
        float hW = hH * aspect_ratio;

        glm::vec3 r = right * hW;
        glm::vec3 u = up * hH;

        frustum.points[Frustum::NTL] = near_point - r + u;
        frustum.points[Frustum::NTR] = near_point + r + u;
        frustum.points[Frustum::NBL] = near_point - r - u;
        frustum.points[Frustum::NBR] = near_point + r - u;

        float far_distance = far_plane + near_plane;
        hH = tan_fov * far_distance;
        hW = aspect_ratio * hH;
        r = right * hW;
        u = up * hH;

        glm::vec3 far_point = position - far_distance * forward;
        frustum.points[Frustum::FTL] = far_point - r + u;
        frustum.points[Frustum::FTR] = far_point + r + u;
        frustum.points[Frustum::FBL] = far_point - r - u;
        frustum.points[Frustum::FBR] = far_point + r - u;
        */
    }

    void Camera::update_projection_matrix() {
        if (projection_mode == PROJECTION_MODE_PERSPECTIVE) {
            projection_matrix = glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
        } else {
            float cam_dist = glm::length(position + forward * near_plane);
            float y_span = cam_dist * tan(glm::radians(fov * 0.5f));
            float x_span = y_span * aspect_ratio;
            projection_matrix = glm::ortho(-x_span, x_span, -y_span, y_span, near_plane, far_plane);
        }

        // Used for TAA
        // Update the translation component
        projection_matrix[2][0] += jitter_factor.x;
        projection_matrix[2][1] += jitter_factor.y;

        inv_projection_matrix = glm::inverse(projection_matrix);
    }

} // namespace mirai