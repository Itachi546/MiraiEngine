#pragma once

#include "Scene/Camera.hpp"
#include "Device/InputDevice.hpp"
#include "Device/Window.hpp"

using namespace mirai;

class FirstPersonController {
  public:
    FirstPersonController(Camera *camera) {
        this->camera = camera;
        target_position = camera->position;
        target_rotation = camera->rotation;
    }

    void set_disable_input(bool state) {
        disable_input = state;
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

    void set_smoothing_enabled(bool state) {
        this->enable_smoothing = state;
    }

    // Enable smooth camera movement
    bool is_smoothing_enabled() const {
        return enable_smoothing;
    }

    // Smoothing factor is between 0 and 1
    void set_smoothing_factor(float factor) {
        smoothing_factor = factor;
    }

    float get_smoothing_factor() const {
        return smoothing_factor;
    }

    void set_rotation_smoothing_factor(float factor) {
        rotation_smoothing_factor = factor;
    }

    float get_rotation_smoothing_factor() const {
        return rotation_smoothing_factor;
    }

    void update(float dt);

    Camera *camera;
    bool disable_input = false;
    float walk_speed = 5.0f;
    float run_speed = 10.0f;
    float sensitivity = 12.0f;
    bool enable_smoothing = true;

    float smoothing_factor = 0.1f;
    float rotation_smoothing_factor = 0.04f;

    glm::vec3 target_position;
    glm::vec3 target_rotation;
};