#include "DeferredPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderHashMap.hpp"
namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass") {
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        push_constant_data.last_frame_VP = renderer->prev_frame_VP;
    }

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("Deferred Render");
        ScopedGpuProfiling(command_buffer, "Deferred Pass");

        device->begin_debug_utils_label(command_buffer, "Deferred Pass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = renderer->main_render_batches;

        push_constant_data.last_frame_VP = renderer->prev_frame_VP;
        push_constant_data.prev_frame_jitter = renderer->prev_frame_jitter;
        push_constant_data.current_frame_jitter = renderer->current_frame_jitter;

        PushConstant push_constant = {
            .data = &push_constant_data,
            .offset = 0,
            .size = sizeof(PushConstantData),
            .shader_stage = SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT,
        };

        auto draw_batch_skinned = [&](RenderBatchType render_batch_type, PipelineState &pipeline_state) {
            for (auto &batch : render_batches) {
                if (batch.batch_type != render_batch_type)
                    continue;

                pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
                pipeline_state.render_state.fields.pass_mode = SHADER_PASS_PBR_DEFERRED_SKINNED;
                Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());

                if (shader == nullptr) {
                    Log::Fatal("Failed to load deferred pipeline shader");
                }

                shader->bind(command_buffer);

                uint32_t current_frame = device->get_current_frame();
                Skeleton &skeleton = renderer->get_scene()->skeletons[0];
                uint32_t total_matrices = cast_u32(skeleton.parents.size());
                uint32_t staging_buffer_offset = renderer->allocate_staging_buffer(total_matrices * sizeof(glm::mat4), current_frame);

                uint8_t *matrix_palletes = reinterpret_cast<uint8_t *>(renderer->per_frame_staging_buffer_ptr + staging_buffer_offset);
                std::memcpy(matrix_palletes, skeleton.current_pose.matrix_palletes.data(), sizeof(glm::mat4) * total_matrices);

                UniformLayout layouts = {
                    .binding = 0,
                    .binding_type = BINDING_TYPE_STORAGE_BUFFER,
                    .shader_stage = SHADER_STAGE_VERTEX,
                };

                UniformSetID uniform_set = command_buffer->create_uniform_set(&layouts, 1, 5);
                UniformBinding binding = {
                    .resource_id = renderer->per_frame_staging_buffer,
                    .buffer_info = {
                        .offset = staging_buffer_offset,
                        .range = sizeof(glm::mat4) * total_matrices,
                    },
                };
                device->update_uniform_set(uniform_set, &binding, 1);

                UniformSetID uniform_sets[] = {
                    renderer->vt_per_frame_uniform_set,
                    renderer->transform_material_set,
                    uniform_set,
                };

                command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
                command_buffer->set_push_constants(shader->pipeline_id, &push_constant, 1);

                for (auto &mesh_batch : batch.meshes) {
                    DrawBatch(command_buffer, &mesh_batch, shader);
                }
            }
        };

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

                UniformSetID uniform_sets[] = {
                    renderer->vt_per_frame_uniform_set,
                    renderer->transform_material_set,
                };

                command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
                command_buffer->set_push_constants(shader->pipeline_id, &push_constant, 1);

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

            draw_batch_skinned(RENDERBATCH_TYPE_SKINNED, pipeline_state);

            pipeline_state.render_state.fields.cull_mode = CULL_MODE_NONE;
            draw_batch(RENDERBATCH_TYPE_ALPHA_MASK, pipeline_state);
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DeferredPass::~DeferredPass() {
    }
} // namespace mirai