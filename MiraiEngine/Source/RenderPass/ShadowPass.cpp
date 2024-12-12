#include "ShadowPass.hpp"

#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Component.hpp"
#include "Scene/Camera.hpp"

namespace mirai {

    void CascadedShadowPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_shared<ShaderMaterial>("cascaded_shadow_material");
        shader->create_from_file({
            "SPIRV/cascaded_shadow.vert.spv",
            "SPIRV/cascaded_shadow.geom.spv",
        });

        shadow_map_size = node->width;
        split_distances.resize(cascade_count);
        cascade_VP.resize(cascade_count);
    }

    void CascadedShadowPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
        Light *sun = scene->get_sun();
        ASSERT(sun->cast_shadow);

        glm::vec3 light_direction = sun->direction;
        Camera *camera = scene->get_camera();

        const Frustum &frustum = camera->get_frustum();
        std::array<glm::vec3, 8> camera_frustum_points = frustum.points;

        calculate_split_distance(camera->get_near_plane(), camera->get_far_plane());

        float last_split_distance = 0.0f;
        for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {
            float current_split_distance = split_distances[cascade];

            std::array<glm::vec3, 8> frustum_corners;
            glm::vec3 frustum_center = glm::vec3(0.0f);
            for (uint32_t i = 0; i < 4; ++i) {
                glm::vec3 direction = camera_frustum_points[i + 4] - camera_frustum_points[i];
                frustum_corners[i] = last_split_distance * direction;
                frustum_corners[i + 4] = current_split_distance * direction;
                frustum_center += frustum_corners[i] + frustum_corners[i + 4];
            }

            frustum_center /= 8.0f;

            // Calculate view_matrix
            glm::mat4 view_matrix = glm::lookAt(frustum_center + light_direction, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));

            // Calculate bounding box

            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            for (uint32_t i = 0; i < 8; ++i) {
                // Project the frustum corner in light view space
                glm::vec3 projected_corner = view_matrix * glm::vec4(frustum_corners[i], 1.0f);
                min = glm::min(min, projected_corner);
                max = glm::max(max, projected_corner);
            }

            glm::mat4 projection_matrix = glm::ortho(min.x, max.x, min.y, max.y, min.z, max.z);
            cascade_VP[cascade] = projection_matrix * view_matrix;
            last_split_distance = current_split_distance;
        }
    }

    void CascadedShadowPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
    }

    void CascadedShadowPass::calculate_split_distance(float znear, float zfar) {
        float ratio = zfar / znear;
        float range = zfar - znear;
        float lamdaN = cascade_count * split_lamda;
        float one_minus_lambda = 1.0f - split_lamda;

        for (uint32_t i = 0; i < cascade_count; ++i) {
            float exponent = float(i + 1) / cascade_count;
            float distance = lamdaN * std::pow(ratio, exponent) + one_minus_lambda * (znear + exponent) * range;
            split_distances[i] = distance / range;
        }
    }

} // namespace mirai