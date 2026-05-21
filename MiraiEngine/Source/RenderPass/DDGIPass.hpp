#pragma once

#include "Math/Math.hpp"
#include "Common/CommonInclude.hpp"

namespace mirai {

    struct IrradianceFieldSettings {
        glm::uvec3 probe_counts = glm::uvec3(4, 2, 4);
        AABB probe_dimension = AABB{glm::vec3(0.0f), glm::vec3(1.0f)};

        glm::vec3 probe_start_position;
        glm::vec3 probe_step;

        IrradianceFieldSettings() {
            ASSERT(glm::greaterThan(probe_counts, glm::uvec3(1)));
            probe_start_position = probe_dimension.min;
            probe_step = (probe_dimension.max - probe_dimension.min) / glm::vec3(probe_counts - glm::uvec3(1));
        }

        uint32_t get_probe_count() {
            return probe_counts.x * probe_counts.y * probe_counts.z;
        }

        glm::vec3 probe_index_to_position(uint32_t index) {
            glm::uvec3 grid_pos;
            grid_pos.x = index % probe_counts.x;
            grid_pos.y = (index % (probe_counts.x * probe_counts.y)) / probe_counts.x;
            grid_pos.z = index / (probe_counts.x * probe_counts.y);

            return probe_start_position + glm::vec3(grid_pos) * probe_step;
        }
    };
} // namespace mirai