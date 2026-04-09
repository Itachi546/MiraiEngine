#pragma once

#include "Common/CommonInclude.hpp"
#include "Math/Math.hpp"

namespace mirai {
    class Scene;

    constexpr const uint32_t NUM_DIRLIGHT_CASCADE = 4;
    struct DirectionLightShadowParams {
        bool enabled;
        float split_lambda;
        float shadow_distance;
        uint32_t shadow_map_size;

        bool calculate_distance_automatic;
        float split_distances[NUM_DIRLIGHT_CASCADE];
    };

    struct DirectionalLightCascadeInfo {
        glm::mat4 VP[NUM_DIRLIGHT_CASCADE];
        float split_distances[4];
        float z_range;
        float width;
        float height;
    };

    class ShadowSystem {
      public:
        ShadowSystem();

        ShadowSystem(const ShadowSystem &) = delete;
        ShadowSystem(ShadowSystem &&) = delete;
        ShadowSystem operator=(const ShadowSystem &) = delete;
        ShadowSystem operator=(ShadowSystem &&) = delete;

        void update(Scene *scene);

        static ShadowSystem *get() {
            return Instance;
        }

        DirectionLightShadowParams dir_light_params = {
            .enabled = true,
            .split_lambda = 0.909f,
            .shadow_distance = 100.0f,
            .shadow_map_size = 1024,
            .calculate_distance_automatic = true,
            .split_distances = {5.0f, 15.0f, 40.0f, 100.0f},
        };
        DirectionalLightCascadeInfo cascade_info;

      private:
        static ShadowSystem *Instance;

        void update_directional_cascade(Scene *scene);

        void calculate_split_distances(float znear, float zfar);
    };

} // namespace mirai