#include "ShadowSystem.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"

namespace mirai {
    ShadowSystem *ShadowSystem::Instance = nullptr;
    ShadowSystem::ShadowSystem() {
        ASSERT(Instance == nullptr);
        Instance = this;
    }

    void ShadowSystem::update_directional_cascade(Scene *scene) {
        Entity sun = scene->get_default_directional_light();
        Camera *camera = scene->get_camera();

        LightComponent *light_component = scene->ecs->component_manager->get_component<LightComponent>(sun);
        dir_light_params.enabled = light_component->cast_shadow;
        if (!light_component->cast_shadow)
            return;

        TransformComponent *light_transform = scene->ecs->component_manager->get_component<TransformComponent>(sun);
        glm::vec3 light_direction = quat_to_direction(light_transform->rotation);

        float z_near = camera->get_near_plane();
        float z_far = dir_light_params.shadow_distance;

        calculate_split_distances(z_near, z_far);

        float last_split_distance = z_near;
        float z_range = cascade_info.z_range;
        float fov = glm::radians(camera->get_fov());
        float aspect_ratio = camera->get_aspect_ratio();
        glm::mat4 V = camera->get_view_transform();

        for (int cascade = 0; cascade < NUM_DIRLIGHT_CASCADE; ++cascade) {
            float split_distance = cascade_info.split_distances[cascade] * z_range;
            glm::mat4 P = glm::perspective(fov, aspect_ratio, last_split_distance, split_distance);

            glm::mat4 VP = P * V;

            std::array<glm::vec3, 8> frustum_corners;
            FrustumPoints::calculate_frustum_corners(glm::inverse(VP), frustum_corners);

            glm::vec3 center{0.0f};
            for (const auto &corner : frustum_corners)
                center += corner;
            center /= static_cast<float>(frustum_corners.size());

            float radius = 0.0f;
            for (const auto &v : frustum_corners) {
                float dist = glm::distance(v, center);
                radius = glm::max(radius, dist);
            }
            radius = std::ceil(radius * 16.0f) / 16.0f;

            glm::vec3 max_extents{radius};
            glm::vec3 min_extents{-max_extents};

            glm::mat4 light_view_transform = glm::lookAt(center - light_direction * min_extents.z, center, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 light_projection_transform = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);
            cascade_info.VP[cascade] = light_projection_transform * light_view_transform;
            last_split_distance = split_distance;
        }

        cascade_info.width = cast_float(dir_light_params.split_size);
        cascade_info.height = cast_float(dir_light_params.split_size);
    }

    void ShadowSystem::update(Scene *scene) {
        ScopedCpuProfiling("CSM Update");
        update_directional_cascade(scene);
    }

    void ShadowSystem::calculate_split_distances(float znear, float zfar) {
        if (dir_light_params.calculate_distance_automatic) {
            float ratio = zfar / znear;
            float range = zfar - znear;

            for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                float p = (i + 1) / float(NUM_DIRLIGHT_CASCADE);
                float log = znear * std::pow(ratio, p);
                float uniform = znear + range * p;

                float d = dir_light_params.split_lambda * (log - uniform) + uniform;
                cascade_info.split_distances[i] = (d - znear) / range;
            }
            cascade_info.z_range = range;
        } else {
            float z_range = dir_light_params.split_distances[NUM_DIRLIGHT_CASCADE - 1];
            for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                cascade_info.split_distances[i] = dir_light_params.split_distances[i] / z_range;
            }
            cascade_info.z_range = z_range;
        }
    }
} // namespace mirai