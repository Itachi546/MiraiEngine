#pragma once

#include "Scene/Camera.hpp"
#include "Device/InputDevice.hpp"
#include "Device/Window.hpp"

using namespace mirai;

class CameraController {
  public:
    CameraController(Camera *camera) {
        this->camera = camera;
        camera->position = glm::vec3(0.0f, 10.0f, 0.0f);
    }

    void set_walk_speed(float speed) {
        this->walk_speed = speed;
    }

    void set_sensitivity(float sensitivity) {
        this->sensitivity = sensitivity;
    }

    float get_walk_speed() const {
        return walk_speed;
    }

    float get_sensitivity() const {
        return sensitivity;
    }

    void set_run_speed(float speed) {
        this->run_speed = speed;
    }

    void update(float dt);

  private:
    Camera *camera;
    float walk_speed = 0.01f;
    float run_speed = 0.1f;
    float sensitivity = 0.01f;
};