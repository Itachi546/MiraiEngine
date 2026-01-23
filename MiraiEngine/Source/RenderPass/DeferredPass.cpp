#include "DeferredPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Scene/Camera.hpp"
namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass") {
    }

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("Deferred Render");
        ScopedGpuProfiling(command_buffer, "Deferred Pass");

        device->begin_debug_utils_label(command_buffer, "Deferred Pass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = renderer->main_render_batches;

        glm::mat4 last_frame_VP = renderer->get_scene()->get_camera()->get_last_frame_view_projection_matrix();
        PushConstant push_constant = {
            .data = &last_frame_VP[0][0],
            .offset = 0,
            .size = sizeof(glm::mat4),
            .shader_stage = SHADER_STAGE_VERTEX,
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

            pipeline_state.render_state.fields.cull_mode = CULL_MODE_NONE;
            draw_batch(RENDERBATCH_TYPE_ALPHA_MASK, pipeline_state);
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DeferredPass::~DeferredPass() {
    }
} // namespace mirai