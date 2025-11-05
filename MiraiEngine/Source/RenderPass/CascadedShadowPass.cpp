#include "CascadedShadowPass.hpp"

#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Component.hpp"
#include "Scene/Camera.hpp"
#include "Scene/RenderBatch.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {

    void CascadedShadowPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        shader = ShaderManager::get()->get_shader("csm_shadow");
        shadow_map_size = node->width / cast_u32(std::sqrt(NUM_DIRLIGHT_CASCADE));
        UniformLayout mesh_instance_layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_STORAGE_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };

        mesh_instance_set = device->create_uniform_set(&mesh_instance_layout, 1, 1, "shadow_mesh_instance_set");
        UniformBinding binding = {.resource_id = renderer->transform_buffer};
        device->update_uniform_set(mesh_instance_set, &binding, 1);
    }

    void CascadedShadowPass::calculate_split_distances(float znear, float zfar, Scene *scene) {
        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        if (calculate_distance_automatic) {
            float ratio = zfar / znear;
            float range = zfar - znear;

            for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                float p = (i + 1) / float(NUM_DIRLIGHT_CASCADE);
                float log = znear * std::pow(ratio, p);
                float uniform = znear + range * p;

                float d = split_lamda * (log - uniform) + uniform;
                cascade_info.split_distances[i] = (d - znear) / range;
            }
            cascade_info.z_range = range;
        } else {
            float z_range = split_distances_constants[NUM_DIRLIGHT_CASCADE - 1];
            for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                cascade_info.split_distances[i] = split_distances_constants[i] / z_range;
            }
            cascade_info.z_range = z_range;
        }
    }

    void CascadedShadowPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        ScopedCpuProfiling("CSM Update");

        Scene *scene = renderer->get_scene();
        Light *light = scene->get_sun();
        Camera *camera = scene->get_camera();

        float z_near = camera->get_near_plane();
        float z_far = shadow_distance;

        calculate_split_distances(z_near, z_far, scene);

        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        float last_split_distance = z_near;
        float z_range = cascade_info.z_range;
        float fov = glm::radians(camera->get_fov());
        float aspect_ratio = camera->get_aspect_ratio();
        glm::mat4 V = camera->get_view_transform();

        glm::vec3 light_direction = light->get_direction();
        for (int cascade = 0; cascade < NUM_DIRLIGHT_CASCADE; ++cascade) {
            float split_distance = cascade_info.split_distances[cascade] * z_range;
            glm::mat4 P = glm::perspective(fov, aspect_ratio, last_split_distance, split_distance);
            glm::mat4 VP = P * V;

            std::array<glm::vec3, 8> frustum_corners;
            Frustum::calculate_frustum_corners(glm::inverse(VP), frustum_corners);

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

        cascade_info.width = static_cast<float>(shadow_map_size);
        cascade_info.height = static_cast<float>(shadow_map_size);
    }

    void CascadedShadowPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedCpuProfiling("CSM Render");
        ScopedGpuProfiling(command_buffer, "Cascaded Shadow Pass");
        device->begin_debug_utils_label(command_buffer, "CascadedShadowPass", nullptr);

        UniformSetID cascade_uniform_set = renderer->cascade_uniform_set;

        auto draw_batch = [&](RenderBatch *batch, PipelineID pipeline_id, uint32_t cascade_index) {
            command_buffer->set_index_buffer(batch->index_buffer);
            command_buffer->set_uniform_sets(pipeline_id, &batch->vertex_binding_set, 1);

            uint32_t instance_data[] = {0, cascade_index, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};
            for (uint32_t i = 0; i < batch->entities.size(); ++i) {
                instance_data[0] = batch->entities[i];
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
        };

        Scene* scene = renderer->get_scene();
        Viewport viewport = {0, 0, shadow_map_size, shadow_map_size, 0.0f, 1.0f};
        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        Frustum frustum;

        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
            device->begin_debug_utils_label(command_buffer, "SPLIT", nullptr);
            node->renderpass_info.attachment_info[0].load_op = i == 0 ? LOAD_OP_CLEAR : LOAD_OP_LOAD;
            uint32_t y = i / 2;
            uint32_t x = i % 2;
            viewport.x = x * shadow_map_size;
            viewport.y = y * shadow_map_size;

            glm::mat4 &VP = cascade_info.VP[i];
            frustum.create_from_matrix(VP, glm::inverse(VP));

            std::vector<RenderBatch> render_batches;
            DrawBatchGenerator::CreateBatch(scene, &frustum, render_batches, true);

            command_buffer->begin_render_pass(node, frame_graph, &viewport);
            if (render_batches.size() > 0) {
                UniformSetID uniform_sets[] = {cascade_uniform_set, mesh_instance_set};
                shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
                shader->bind(command_buffer, &node->renderpass_info);

                for (auto &batch : render_batches)
                    draw_batch(&batch, shader->get_pipeline_id(), i);
            }
            command_buffer->end_render_pass();
            device->end_debug_utils_label(command_buffer);
        }

        device->end_debug_utils_label(command_buffer);
    }

    CascadedShadowPass::~CascadedShadowPass() {
    }

} // namespace mirai