#pragma once

#include "Math/Math.hpp"

namespace mirai {
    inline glm::vec3 clip_to_world_pos(const glm::vec3 &clip_pos, const glm::mat4 &inv_VP) {
        glm::vec4 world_pos = inv_VP * glm::vec4(clip_pos, 1.0f);
        return glm::vec3(world_pos) / world_pos.w;
    }

    inline glm::vec3 world_to_normalized_window_pos(const glm::vec3 &world_pos, const glm::mat4 VP) {
        glm::vec4 ndc_coord = VP * glm::vec4(world_pos, 1.0f);
        ndc_coord /= ndc_coord.w;

        ndc_coord.x = ndc_coord.x * 0.5f + 0.5f;
        ndc_coord.y = 0.5f - ndc_coord.y * 0.5f;

        return ndc_coord;
    }
} // namespace mirai