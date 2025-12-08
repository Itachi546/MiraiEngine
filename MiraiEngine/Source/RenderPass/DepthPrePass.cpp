#include "DepthPrePass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
namespace mirai {
    DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass"), shader(nullptr) {
    }

    void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        shader = ShaderManager::get()->get_shader("depth_prepass");
        // Mesh Data
        transform_layout = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedCpuProfiling("FrameGraph::DepthPrepass");
        auto draw_batch = [&](RenderBatch *batch, PipelineID pipeline_id) {
            // Set Per Frame Data
            uint32_t push_constant_data[4] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = push_constant_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            UniformSetID transform_set = command_buffer->create_uniform_set(&transform_layout, 1, 1);
            UniformBinding binding = {
                .resource_id = batch->transform_buffer_view.buffer,
                .buffer_info = {
                    .offset = batch->transform_buffer_view.offset,
                    .range = batch->transform_buffer_view.size,
                },
            };
            device->update_uniform_set(transform_set, &binding, 1);

            ScopedCpuProfiling("DepthPrepass::DrawBatch");
            UniformSetID uniform_sets[] = {transform_set, batch->vertex_binding_set};
            command_buffer->set_uniform_sets(shader->get_pipeline_id(), uniform_sets, cast_u32(std::size(uniform_sets)));

            command_buffer->set_index_buffer(batch->index_buffer);
            for (uint32_t i = 0; i < batch->transform_indices.size(); ++i) {
                push_constant_data[0] = i;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
            device->destroy_uniform_sets(&transform_set, 1);
        };

        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);
        shader->set_uniform_sets(&renderer->per_frame_uniform_set, 1);
        shader->bind(command_buffer, &node->renderpass_info);

        Scene *scene = renderer->get_scene();
        std::vector<RenderBatch> &render_batches = scene->main_render_batches;
        if (render_batches.size() > 0) {
            for (auto &batch : render_batches) {
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