#include "DepthPrePass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
namespace mirai {
    DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass") {
    }

    void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        // Mesh Data
        transform_layout = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};

        PipelineState pipeline_state;
        pipeline_state.render_state.fields.depth_test = true;
        pipeline_state.render_state.fields.depth_write = true;
        pipeline_state.render_state.fields.pass_mode = SHADER_PASS_DEPTH_PREPASS;

        shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
        if (shader == nullptr) {
            Log::Fatal("Failed to load pipeline for depth-prepass");
        }
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedCpuProfiling("FrameGraph::DepthPrepass");

        auto draw_batch = [&](MeshBatch *batch, PipelineID pipeline_id) {
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
            command_buffer->set_uniform_sets(pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

            command_buffer->set_index_buffer(batch->index_buffer.buffer);
            for (uint32_t i = 0; i < batch->transform_indices.size(); ++i) {
                push_constant_data[0] = i;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
        };

        // Create PerFrame uniform set
        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };
        per_frame_uniform_set = command_buffer->create_uniform_set(&layout, 1, 0);

        UniformBinding binding = {
            .resource_id = renderer->per_frame_uniform_buffer.buffer,
            .buffer_info = {
                .offset = renderer->per_frame_uniform_buffer.offset,
                .range = renderer->per_frame_uniform_buffer.size,
            },
        };
        device->update_uniform_set(per_frame_uniform_set, &binding, 1);

        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        shader->bind(command_buffer);
        command_buffer->set_uniform_sets(shader->pipeline_id, &per_frame_uniform_set, 1);

        Scene *scene = renderer->get_scene();
        std::vector<RenderBatch> &render_batches = scene->main_render_batches;
        if (render_batches.size() > 0) {
            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
                    continue;
                for (auto &mesh_batch : batch.meshes)
                    draw_batch(&mesh_batch, shader->pipeline_id);
            }
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DepthPrePass::~DepthPrePass() {
    }
} // namespace mirai