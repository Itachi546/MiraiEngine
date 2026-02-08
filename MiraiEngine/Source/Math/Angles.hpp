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
} // namespace mirai