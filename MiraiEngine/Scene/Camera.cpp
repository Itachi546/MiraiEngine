#include "Camera.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include "Device/Window.hpp"

namespace mirai
{
    Camera::Camera() : position(glm::vec3(0.0f, 0.0f, -3.0f)),
                       rotation(glm::vec3(0.0f)),
                       fov(60.0f),
                       aspect_ratio(4.0f / 3.0f),
                       near_plane(0.5f),
                       far_plane(1000.0f),
                       projection_mode(PROJECTION_MODE_PERSPECTIVE)
    {
        viewport_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
    }

    void Camera::update()
    {
        aspect_ratio = Window::get()->get_aspect_ratio();
        glm::vec3 rotation_radians = glm::radians(rotation);

        glm::mat4 rotation_matrix = glm::eulerAngleZXY(rotation_radians.z, rotation_radians.x, rotation_radians.y);
        view_matrix = rotation_matrix * glm::translate(glm::mat4(1.0f), -position);

        update_projection_matrix();

        right = glm::vec3(rotation_matrix[0][0], rotation_matrix[1][0], rotation_matrix[2][0]);
        up = glm::vec3(rotation_matrix[0][1], rotation_matrix[1][1], rotation_matrix[2][1]);
        forward = glm::vec3(rotation_matrix[0][2], rotation_matrix[1][2], rotation_matrix[2][2]);
    }

    void Camera::update_projection_matrix()
    {
        if (projection_mode == PROJECTION_MODE_PERSPECTIVE)
        {
            projection_matrix = viewport_matrix * glm::perspective(glm::radians(fov), aspect_ratio, near_plane, far_plane);
        }
        else
        {
            float y_span = near_plane * tan(glm::radians(fov));
            float x_span = y_span * aspect_ratio;
            projection_matrix = glm::ortho(-x_span, x_span, y_span, -y_span, near_plane, far_plane);
        }
    }
} // namespace mirai