#pragma once

#include "Math/Math.hpp"

namespace mirai {
    // Rotation is in radians
    inline glm::vec3 rotation_to_direction(glm::vec3 rotation) {
        float yaw = rotation.y;
        float pitch = rotation.x;

        return {
            cos(pitch) * cos(yaw),
            sin(pitch),
            cos(pitch) * sin(yaw),
        };
    }

    inline glm::vec3 quat_to_direction(glm::fquat rotation) {
        return glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
    }
} // namespace mirai