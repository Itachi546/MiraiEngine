#include "DepthPrePass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass"), shader(nullptr), transform_set(K_INVALID_ID) {
    }

    void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
        shader = ShaderManager::get()->get_shader("depth_prepass");
        // Mesh Data
        UniformLayout transform_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        transform_set = device->create_uniform_set(transform_layout, 1, 1, "depth_prepass_uniform_set");
        UniformBinding per_shader_bindings[] = {
            {.resource_id = scene->transform_buffer},
        };

        device->update_uniform_set(transform_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        auto draw_batch = [&](RenderBatch *batch, PipelineID pipeline_id) {
            // Set Per Frame Data
            uint32_t push_constant_data[4] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = push_constant_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};
            command_buffer->set_index_buffer(batch->index_buffer);
            command_buffer->set_uniform_sets(pipeline_id, &batch->vertex_binding_set, 1);
            for (uint32_t i = 0; i < batch->transform_indices.size(); ++i) {
                push_constant_data[0] = batch->transform_indices[i];
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
        };

        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = scene->main_render_batches;
        if (render_batches.size() > 0) {
            for (auto &batch : render_batches) {
                UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, transform_set};
                shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
                shader->bind(command_buffer, &node->renderpass_info);
                if (batch.batch_type == RENDERBATCH_TYPE_OPAQUE)
                    draw_batch(&batch, shader->get_pipeline_id());
            }
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DepthPrePass::~DepthPrePass() {
    }
} // namespace mirai