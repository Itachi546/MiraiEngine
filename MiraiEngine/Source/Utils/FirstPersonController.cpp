#include "FirstPersonController.hpp"

void FirstPersonController::update(float dt) {
    Input *input = Input::get();
    if (!disable_input) {
        float speed = walk_speed;
        if (input->is_down(KB_LEFT_SHIFT))
            speed = run_speed;

        glm::vec3 direction{0.0f};

        if (input->is_down(KB_W))
            direction.z = -1.0f;
        else if (input->is_down(KB_S))
            direction.z = 1.0f;

        if (input->is_down(KB_A))
            direction.x = -1.0f;
        else if (input->is_down(KB_D))
            direction.x = 1.0f;
        if (input->is_down(KB_E))
            direction.y = -1.0f;
        else if (input->is_down(KB_Q))
            direction.y = 1.0f;

        direction = direction.x * camera->get_right() + direction.y * camera->get_up() + direction.z * camera->get_forward();
        target_position += direction * speed * dt;
    }

    if (enable_smoothing) {
    } else
        camera->position = target_position;
    /*
    if (input->is_down(KB_P))
        camera->set_projection_mode(PROJECTION_MODE_PERSPECTIVE);
    else if (input->is_down(KB_O))
        camera->set_projection_mode(PROJECTION_MODE_ORTHOGRAPHIC);
    */
    if (input->is_down(MB_LEFT) && !disable_input) {
        glm::vec2 mouse_delta = Window::get()->get_mouse_delta() * sensitivity;
        target_rotation += glm::vec3(mouse_delta.y, mouse_delta.x, 0.0f) * dt;
        target_rotation.x = glm::clamp(target_rotation.x, -89.0f, 89.0f);
    }

    if (enable_smoothing) {
        glm::vec3 delta = target_position - camera->position;
        camera->position += delta * smoothing_factor;

        glm::vec3 rot_delta = target_rotation - camera->rotation;
        camera->rotation += rot_delta * rotation_smoothing_factor;
    } else {
        camera->position = target_position;
        camera->rotation = target_rotation;
    }
}