#include "ForwardPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Scene/RenderBatch.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {
    ForwardPass::ForwardPass() : FrameGraphRenderer("forward_pass") {
    }

    void ForwardPass::initialize(FrameGraph *framegraph, const FrameGraphNode *node, Renderer *renderer) {
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("FrameGraph::ForwardPass");

        ScopedGpuProfiling(command_buffer, "Forward Pass");

        device->begin_debug_utils_label(command_buffer, "ForwardPass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        PipelineState pipeline_state = {};
        pipeline_state.render_state.fields.depth_test = true;
        pipeline_state.render_state.fields.depth_write = false;

        Scene *scene = renderer->get_scene();
        std::vector<RenderBatch> &render_batches = scene->main_render_batches;

        // Draw Opaque Object
        if (render_batches.size() > 0) {
            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
                    continue;

                pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
                pipeline_state.render_state.fields.pass_mode = batch.shader_key.fields.shader_pass;
                Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
                if (shader == nullptr) {
                    Log::Fatal("Failed to load forward transparent pipeline");
                }

                shader->bind(command_buffer);
                command_buffer->set_uniform_sets(shader->pipeline_id, &renderer->per_frame_uniform_set, 1);

                for (auto &mesh_batch : batch.meshes) {
                    DrawBatch(command_buffer, &mesh_batch, shader);
                }
            }

            // Draw Transparent Object
            pipeline_state.render_state.fields.cull_mode = CULL_MODE_NONE;
            pipeline_state.render_state.fields.blend_mode = true;
            pipeline_state.render_state.fields.depth_write = true;
            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_OPAQUE)
                    continue;

                pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
                pipeline_state.render_state.fields.pass_mode = batch.shader_key.fields.shader_pass;
                Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
                if (shader == nullptr) {
                    Log::Fatal("Failed to load forward transparent pipeline");
                }
                shader->bind(command_buffer);
                command_buffer->set_uniform_sets(shader->pipeline_id, &renderer->per_frame_uniform_set, 1);
                for (auto &mesh_batch : batch.meshes) {
                    DrawBatch(command_buffer, &mesh_batch, shader);
                }
            }
        }
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    ForwardPass::~ForwardPass() {
    }
} // namespace mirai