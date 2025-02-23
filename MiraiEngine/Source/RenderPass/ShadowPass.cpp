#include "ShadowPass.hpp"

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
        shader->set_depth_clamp(true);

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

        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
            float p = (i + 1) / float(NUM_DIRLIGHT_CASCADE);
            float log = znear * std::pow(ratio, p);
            float uniform = znear + range * p;

            float d = split_lamda * (log - uniform) + uniform;
            cascade_info.split_distances[i] = (d - znear) / range;
        }
        cascade_info.z_range = range;
    }

    void CascadedShadowPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
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

            glm::mat4 light_view_transform = glm::lookAt(center - normalize(light->direction) * min_extents.z, center, glm::vec3(0.0f, 1.0f, 0.0f));
            glm::mat4 light_projection_transform = glm::ortho(min_extents.x, max_extents.x, min_extents.y, max_extents.y, 0.0f, max_extents.z - min_extents.z);
            cascade_info.VP[cascade] = light_projection_transform * light_view_transform;

            last_split_distance = split_distance;
        }

        cascade_info.width = static_cast<float>(shadow_map_size);
        cascade_info.height = static_cast<float>(shadow_map_size);
        DirectionalLightInfo &light_info = scene->directional_light_info;
        std::memcpy(light_info.cascade_buffer_ptr, &cascade_info, sizeof(cascade_info));

        // @TODO Generate draw batches
        if (opaque_batches.size() == 0)
            scene->generate_draw_batch(opaque_batches, transparent_batches, nullptr);
    }

    void CascadedShadowPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "CascadedShadowPass", nullptr);
        ScopedGpuProfiling(command_buffer, "Cascaded Shadow Pass");

        UniformBinding binding = {.resource_id = scene->transform_buffer};
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

        draw_batch(opaque_batches.data(), static_cast<uint32_t>(opaque_batches.size()), shader.get());

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    CascadedShadowPass::~CascadedShadowPass() {
    }

} // namespace mirai