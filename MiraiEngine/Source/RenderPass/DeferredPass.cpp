#include "DeferredPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderHashMap.hpp"
namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass") {
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        UniformLayout layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };
        transform_material_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 3, "transform_material_set");

        UniformBinding bindings[] = {
            {.resource_id = renderer->global_transform_buffer},
            {.resource_id = renderer->global_material_buffer},
        };
        device->update_uniform_set(transform_material_set, bindings, cast_u32(std::size(bindings)));
    }

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("Deferred Render");
        ScopedGpuProfiling(command_buffer, "Deferred Pass");

        device->begin_debug_utils_label(command_buffer, "Deferred Pass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = renderer->main_render_batches;

        auto draw_batch = [&](RenderBatchType render_batch_type, PipelineState &pipeline_state) {
            for (auto &batch : render_batches) {
                if (batch.batch_type != render_batch_type)
                    continue;
                pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
                pipeline_state.render_state.fields.pass_mode = batch.shader_key.fields.shader_pass;
                Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());

                if (shader == nullptr) {
                    Log::Fatal("Failed to load deferred pipeline shader");
                }

                shader->bind(command_buffer);

                // Create PerFrame uniform set
                UniformLayout layout = {
                    .binding = 0,
                    .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
                    .shader_stage = SHADER_STAGE_VERTEX,
                };

                UniformSetID per_frame_uniform_set = command_buffer->create_uniform_set(&layout, 1, 0);
                UniformBinding binding = {
                    .resource_id = renderer->per_frame_uniform_buffer.buffer,
                    .buffer_info = {
                        .offset = renderer->per_frame_uniform_buffer.offset,
                        .range = renderer->per_frame_uniform_buffer.size,
                    },
                };
                device->update_uniform_set(per_frame_uniform_set, &binding, 1);

                UniformSetID uniform_sets[] = {
                    per_frame_uniform_set,
                    transform_material_set,
                };
                command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

                for (auto &mesh_batch : batch.meshes) {
                    DrawBatch(command_buffer, &mesh_batch, shader);
                }
            }
        };

        if (render_batches.size() > 0) {
            // Draw Opaque Object
            PipelineState pipeline_state = {};
            pipeline_state.render_state.fields.depth_test = true;
            pipeline_state.render_state.fields.depth_write = true;
            pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;
            draw_batch(RENDERBATCH_TYPE_OPAQUE, pipeline_state);
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DeferredPass::~DeferredPass() {
    }
} // namespace mirai