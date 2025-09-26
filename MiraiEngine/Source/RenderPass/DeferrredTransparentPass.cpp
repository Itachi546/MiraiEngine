#include "DeferredTransparentPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/RenderBatch.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    DeferredTransparentPass::DeferredTransparentPass() : FrameGraphRenderer("forward_pass"), transparent_shader(nullptr), mesh_instance_set(K_INVALID_ID) {
    }

    void DeferredTransparentPass::initialize(FrameGraph *framegraph, const FrameGraphNode *node, Scene *scene) {
        transparent_shader = ShaderManager::get()->get_shader("pbr_transparent");

        // Mesh Data
        UniformLayout mesh_data_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        // Mesh Instance Data (Transform/Material)
        UniformLayout mesh_instance_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };

        mesh_instance_set = device->create_uniform_set(mesh_instance_layout, (uint32_t)std::size(mesh_instance_layout), 3, "mesh_instance_set");
        // Update Per Pipeline Data (Transform/Material)
        UniformBinding per_shader_bindings[] = {
            {.resource_id = scene->transform_buffer},
            {.resource_id = scene->material_buffer},
        };
        device->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));
    }

    void DeferredTransparentPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        auto draw_batch = [&](RenderBatch *batch, PipelineID pipeline_id) {
            // Set Per Frame Data
            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant_vert = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};
            command_buffer->set_index_buffer(batch->index_buffer);
            command_buffer->set_uniform_sets(pipeline_id, &batch->vertex_binding_set, 1);
            for (uint32_t i = 0; i < batch->transform_indices.size(); ++i) {
                instance_data[0] = batch->transform_indices[i];
                instance_data[1] = batch->material_indices[i];
                command_buffer->set_push_constants(pipeline_id, &push_constant_vert, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
        };

        ScopedGpuProfiling(command_buffer, "Deferred TransparentPass");

        device->begin_debug_utils_label(command_buffer, "DeferredTransparent", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = scene->main_render_batches;
        if (render_batches.size() > 0) {
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, mesh_instance_set};
            transparent_shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            transparent_shader->bind(command_buffer, &node->renderpass_info);

            PushConstant push_constant_frag = {
                .data = &scene->scene_data,
                .shader_stage = SHADER_STAGE_FRAGMENT,
                .size = sizeof(scene->scene_data),
                .offset = sizeof(uint32_t) * 4,
            };
            PipelineID pipeline_id = transparent_shader->get_pipeline_id();
            command_buffer->set_push_constants(pipeline_id, &push_constant_frag, 1);

            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT) {
                    draw_batch(&batch, pipeline_id);
                }
            }
        }
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DeferredTransparentPass::~DeferredTransparentPass() {
    }
} // namespace mirai