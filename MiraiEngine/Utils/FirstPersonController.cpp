#include "FirstPersonController.hpp"

void FirstPersonController::update(float dt) {
    Input *input = Input::get();
    float speed = walk_speed;

    if (input->is_down(KB_LEFT_SHIFT))
        speed = run_speed;
    speed *= dt;

    if (input->is_down(KB_W))
        target_position -= camera->get_forward() * speed;
    else if (input->is_down(KB_S))
        target_position += camera->get_forward() * speed;

    if (input->is_down(KB_A))
        target_position -= camera->get_right() * speed;
    else if (input->is_down(KB_D))
        target_position += camera->get_right() * speed;

    if (input->is_down(KB_E))
        target_position -= camera->get_up() * speed;
    else if (input->is_down(KB_Q))
        target_position += camera->get_up() * speed;

    if (input->is_down(KB_P))
        camera->set_projection_mode(PROJECTION_MODE_PERSPECTIVE);
    else if (input->is_down(KB_O))
        camera->set_projection_mode(PROJECTION_MODE_ORTHOGRAPHIC);

    if (input->is_down(MB_LEFT)) {
        glm::vec2 mouse_delta = Window::get()->get_mouse_delta() * sensitivity * dt;
        // mouse_delta.y = 0.0f;
        camera->rotation += glm::vec3(mouse_delta.y, mouse_delta.x, 0.0f);
        camera->rotation.x = glm::clamp(camera->rotation.x, -89.0f, 89.0f);
    }
    /*
    if (enable_smoothing) {
        glm::vec3 delta = target_position - camera->position;
        if (glm::dot(delta, delta) < 0.001f)
            camera->position = target_position;
        else
            camera->position += delta * glm::min(smoothing_factor, 1.0f);
    } else
    */
    camera->position = target_position;
}