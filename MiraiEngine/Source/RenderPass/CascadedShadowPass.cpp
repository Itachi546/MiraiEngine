#include "CascadedShadowPass.hpp"

#include "Scene/ShaderHashMap.hpp"
#include "Scene/Component.hpp"
#include "Scene/Camera.hpp"
#include "Scene/RenderBatch.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {

    void CascadedShadowPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        // Mesh Data
        PipelineState pipeline_state;
        pipeline_state.render_state.fields.depth_test = true;
        pipeline_state.render_state.fields.depth_write = true;
        pipeline_state.render_state.fields.pass_mode = SHADER_PASS_CASCADED_SHADOW;
        pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;

        PipelineAttachmentInfo attachment_info = {
            .has_depth_attachment = true,
            .depth_attachment_format = FORMAT_D32_SFLOAT,
        };
        shader = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/cascaded-shadow.vert.spv"}, "cascaded-shadow-map-shader");
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

    static UniformLayout DRAW_DATA_LAYOUT = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};
    void CascadedShadowPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedCpuProfiling("CSM Render");
        ScopedGpuProfiling(command_buffer, "Cascaded Shadow Pass");
        device->begin_debug_utils_label(command_buffer, "CascadedShadowPass", nullptr);
        uint32_t current_frame = device->get_current_frame();

        auto draw_batch = [&renderer, current_frame](CommandBuffer *command_buffer, MeshBatch *batch, PipelineID pipeline_id) {
            if (batch->mesh_draw_infos.size() == 0)
                return;

            RenderingDevice *device = RenderingDevice::get();

            uint32_t num_entity = cast_u32(batch->mesh_draw_infos.size());

            uint32_t draw_data_instance_size = sizeof(uint32_t);
            uint32_t draw_data_size_bytes = num_entity * draw_data_instance_size;
            uint32_t draw_data_offset = renderer->allocate_staging_buffer(draw_data_size_bytes, current_frame);
            uint32_t *draw_data_array = reinterpret_cast<uint32_t *>(renderer->per_frame_staging_buffer_ptr + draw_data_offset);

            uint32_t indirect_data_size_bytes = num_entity * sizeof(DrawIndexedIndirectCommand);
            uint32_t indirect_data_offset = renderer->allocate_staging_buffer(indirect_data_size_bytes, current_frame);
            uint8_t *indirect_data_array = (renderer->per_frame_staging_buffer_ptr + indirect_data_offset);

            /**
             * Maybe the right way to do this is to do GPU Culling and generating
             * indirect command.
             * This way we can reuse same portion of staging buffer memory for other
             * cascade as well instead of allocation new portion of memory for each cascade.
             */
            for (auto &draw_info : batch->mesh_draw_infos) {
                std::memcpy(indirect_data_array, &draw_info.draw_info, sizeof(DrawIndexedIndirectCommand));
                indirect_data_array += sizeof(DrawIndexedIndirectCommand);

                *draw_data_array = draw_info.transform_index;
                draw_data_array += draw_data_instance_size;
            }

            UniformSetID draw_data_set = command_buffer->create_uniform_set(&DRAW_DATA_LAYOUT, 1, 1);
            UniformBinding draw_data_binding = {
                .resource_id = renderer->per_frame_staging_buffer,
                .buffer_info{
                    .offset = draw_data_offset,
                    .range = draw_data_size_bytes,
                },
            };
            device->update_uniform_set(draw_data_set, &draw_data_binding, 1);

            UniformSetID uniform_sets[2] = {
                batch->vertex_binding_set,
                draw_data_set,
            };
            command_buffer->set_uniform_sets(pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

            command_buffer->set_index_buffer(batch->index_buffer.buffer);
            command_buffer->draw_indexed_indirect(renderer->per_frame_staging_buffer, indirect_data_offset, num_entity, sizeof(DrawIndexedIndirectCommand));
        };

        Scene *scene = renderer->get_scene();
        Viewport viewport = {0, 0, shadow_map_size, shadow_map_size, 0.0f, 1.0f};
        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        Frustum frustum;

        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };
        UniformSetID cascade_uniform_set = command_buffer->create_uniform_set(&layout, 1, 0);
        UniformBinding binding = {
            .resource_id = renderer->cascade_uniform_buffer.buffer,
            .buffer_info = {
                .offset = renderer->cascade_uniform_buffer.offset,
                .range = renderer->cascade_uniform_buffer.size,
            },
        };

        device->update_uniform_set(cascade_uniform_set, &binding, 1);
        UniformSetID uniform_sets[] = {cascade_uniform_set, renderer->transform_set};
        uint32_t push_constant_data[] = {0, 0, 0, 0};
        PushConstant push_constant = {
            .data = &push_constant_data,
            .offset = 0,
            .size = sizeof(uint32_t) * 4,
            .shader_stage = SHADER_STAGE_VERTEX,
        };

        /**
         * The i == 0  check for clearing the texture doesn't works if nothing is
         * inside the frustum. This miss the clearing of texture entirely (LOAD_OP_CLEAR).
         * So we keep track of the first render
         */
        bool first_render = true;
        for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
            device->begin_debug_utils_label(command_buffer, "SPLIT", nullptr);
            // Hack to set the attachment load op for multiple pass rendering to same texture
            node->renderpass_info.attachment_info[0].load_op = first_render ? LOAD_OP_CLEAR : LOAD_OP_LOAD;

            uint32_t y = i / 2;
            uint32_t x = i % 2;
            viewport.x = x * shadow_map_size;
            viewport.y = y * shadow_map_size;

            glm::mat4 &VP = cascade_info.VP[i];
            frustum.create_from_matrix(VP, glm::inverse(VP));

            std::vector<MeshBatch> mesh_batches;
            DrawBatchGenerator::CreateMeshBatch(scene, &frustum, mesh_batches);
            if (mesh_batches.size() > 0) {
                command_buffer->begin_render_pass(node, frame_graph, &viewport);
                shader->bind(command_buffer);
                command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, (uint32_t)std::size(uniform_sets));
                push_constant_data[0] = i;
                command_buffer->set_push_constants(shader->pipeline_id, &push_constant, 1);
                for (auto &batch : mesh_batches)
                    draw_batch(command_buffer, &batch, shader->pipeline_id);

                command_buffer->end_render_pass();

                first_render = false;
            }
            device->end_debug_utils_label(command_buffer);
        }

        device->end_debug_utils_label(command_buffer);
    } // namespace mirai

    CascadedShadowPass::~CascadedShadowPass() {
    }

} // namespace mirai