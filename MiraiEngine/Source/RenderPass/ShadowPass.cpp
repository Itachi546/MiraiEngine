#include "ShadowPass.hpp"

#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Component.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {

    void CascadedShadowPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_shared<ShaderMaterial>("cascaded_shadow_material");
        shader->create_from_file({
            "SPIRV/cascaded_shadow.vert.spv",
            "SPIRV/cascaded_shadow.geom.spv",
        });
        shader->set_depth_write(true);
        shader->set_depth_test(true);

        shadow_map_size = node->width;

        UniformLayout mesh_instance_layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_STORAGE_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };
        mesh_instance_set = device->create_uniform_set(&mesh_instance_layout, 1, 1, "shadow_mesh_instance_set");
    }

    void CascadedShadowPass::calculate_split_distances(float znear, float zfar, Scene *scene) {
        float ratio = zfar / znear;
        float range = zfar - znear;

        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;

        for (uint32_t i = 0; i < cascade_count; ++i) {
            float p = (i + 1) / float(cascade_count);
            float log = znear * std::pow(ratio, p);
            float uniform = znear + range * p;

            float d = split_lamda * (log - uniform) + uniform;
            cascade_info.split_distances[i] = (d - znear) / range;
        }
        cascade_info.z_range = range;
    }

    void CascadedShadowPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
        Light *sun = scene->get_sun();
        ASSERT(sun->cast_shadow);

        Camera *camera = scene->get_camera();

        glm::vec3 light_direction = sun->direction;

        const Frustum &frustum = camera->get_frustum();
        std::array<glm::vec3, 8> camera_frustum_points = frustum.points;

        calculate_split_distances(camera->get_near_plane(), camera->get_far_plane(), scene);

        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        float last_split_distance = 0.0f;
        
        glm::mat4 viewport_matrix = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, -1.0f, 1.0f));
        for (uint32_t cascade = 0; cascade < cascade_count; ++cascade) {

            float split_distance = cascade_info.split_distances[cascade];

            std::array<glm::vec3, 8> frustum_corners;
            for (uint32_t i = 0; i < 4; ++i) {
                glm::vec3 direction = camera_frustum_points[i + 4] - camera_frustum_points[i];
                frustum_corners[i + 4] = camera_frustum_points[i] + split_distance * direction;
                frustum_corners[i] = camera_frustum_points[i] + last_split_distance * direction;
            }

            glm::vec3 frustum_center = frustum_corners[0];
            for (uint32_t i = 1; i < 8; ++i)
                frustum_center += frustum_corners[i];
            frustum_center /= 8.0f;

            // Calculate view_matrix
            //glm::mat4 light_view_matrix = glm::lookAt(glm::vec3(0.0f), -light_direction, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 light_view_matrix = glm::lookAt(frustum_center + light_direction, frustum_center, glm::vec3(0.0f, 1.0f, 0.0f));

            // Calculate bounding box
            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            for (uint32_t i = 0; i < 8; ++i) {
                // Project the frustum corner in light view space
                glm::vec3 projected_corner = light_view_matrix * glm::vec4(frustum_corners[i], 1.0f);

                min.x = std::min(min.x, projected_corner.x);
                min.y = std::min(min.y, projected_corner.y);
                min.z = std::min(min.z, projected_corner.z);

                max.x = std::max(max.x, projected_corner.x);
                max.y = std::max(max.y, projected_corner.y);
                max.z = std::max(max.z, projected_corner.z);
            }

            float z_factor = 2.0f;
            min.z = min.z < 0.0f ? min.z * z_factor : min.z / z_factor;
            max.z = max.z < 0.0f ? max.z / z_factor : max.z * z_factor;

            glm::mat4 light_projection_matrix = glm::ortho(min.x, max.x, min.y, max.y, min.z, max.z);
            cascade_info.VP[cascade] = viewport_matrix * light_projection_matrix * light_view_matrix;
            last_split_distance = split_distance;
        }

        DirectionalLightInfo &light_info = scene->directional_light_info;
        // Copy to uniform buffer
        std::memcpy(light_info.cascade_buffer_ptr, &cascade_info, sizeof(DirectionalLightCascadeInfo));
    }

    void CascadedShadowPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "CascadedShadowPass", nullptr);
        ScopedGpuProfiling(command_buffer, "Cascaded Shadow Pass");

        UniformBinding binding = {.resource_id = scene->transform_buffer, .offset = 0};
        device->update_uniform_set(mesh_instance_set, &binding, 1);

        UniformSetID cascade_uniform_set = scene->directional_light_info.cascade_uniform_set;

        auto draw_batch = [&](DrawData *batches, uint32_t count, ShaderMaterial *shader) {
            // Set Per Frame Data
            UniformSetID uniform_sets[] = {cascade_uniform_set, mesh_instance_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, &node->renderpass_info);

            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            PipelineID pipeline_id = shader->get_pipeline_id();
            uint32_t last_buffer_id = K_INVALID_ID;
            for (uint32_t i = 0; i < count; ++i) {
                BufferID current_buffer = batches[i].vertex_buffer;
                if (current_buffer.id != last_buffer_id) {
                    command_buffer->set_index_buffer(batches[i].index_buffer);
                    last_buffer_id = current_buffer.id;
                    command_buffer->set_uniform_sets(pipeline_id, &batches[i].vertex_binding_set, 1);
                }

                instance_data[0] = batches[i].transform_index;
                instance_data[1] = batches[i].material_index;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batches[i].index_count,
                                             1,
                                             batches[i].index_offset,
                                             batches[i].vertex_offset,
                                             0);
            }
        };

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &draw_data = scene->main_opaque_draw_batch;

        draw_batch(draw_data.data(), static_cast<uint32_t>(draw_data.size()), shader.get());

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    CascadedShadowPass::~CascadedShadowPass() {
    }

} // namespace mirai