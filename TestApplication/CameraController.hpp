#pragma once

#include "Scene/Camera.hpp"
#include "Device/InputDevice.hpp"
#include "Device/Window.hpp"

using namespace mirai;

class CameraController
{
  public:
    CameraController(Camera *camera)
    {
        this->camera = camera;
        camera->position = glm::vec3(0.0f, 0.0f, 3.0f);
    }

    void update(float dt)
    {
        float aspect_ratio = Window::get()->get_aspect_ratio();
        camera->set_aspect_ratio(aspect_ratio);

        Input *input = Input::get();
        float walk_speed = WALK_SPEED * dt;
        if (input->is_down(KB_W))
        {
            camera->position -= camera->get_forward() * walk_speed;
        }
        else if (input->is_down(KB_S))
        {
            camera->position += camera->get_forward() * walk_speed;
        }

        if (input->is_down(KB_A))
        {
            camera->position -= camera->get_right() * walk_speed;
        }
        else if (input->is_down(KB_D))
        {
            camera->position += camera->get_right() * walk_speed;
        }

        if (input->is_down(KB_P))
        {
            camera->set_projection_mode(PROJECTION_MODE_PERSPECTIVE);
        }
        else if (input->is_down(KB_O))
        {
            camera->set_projection_mode(PROJECTION_MODE_ORTHOGRAPHIC);
        }

        if (input->is_down(KB_E))
        {
            camera->position -= camera->get_up() * walk_speed;
        }
        else if (input->is_down(KB_Q))
        {
            camera->position += camera->get_up() * walk_speed;
        }
    }

  private:
    Camera *camera;
    const float WALK_SPEED = 0.02f;
};