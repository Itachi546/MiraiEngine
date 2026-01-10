#include "DepthPrePass.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
namespace mirai {
    DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass") {
    }

    void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        // Mesh Data
        PipelineState pipeline_state;
        pipeline_state.render_state.fields.depth_test = true;
        pipeline_state.render_state.fields.depth_write = true;
        pipeline_state.render_state.fields.pass_mode = SHADER_PASS_DEPTH_PREPASS;
        pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;

        shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
        if (shader == nullptr) {
            Log::Fatal("Failed to load pipeline for depth-prepass");
        }
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        auto draw_batch = [&](MeshBatch *batch, Shader *shader) {
            if (shader->get_draw_mode() == DRAWMODE_INDEXED_INDIRECT) {
                UniformSetID uniform_sets[] = {renderer->vt_per_frame_uniform_set, batch->vertex_binding_set};
                command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
                command_buffer->set_index_buffer(batch->index_buffer.buffer);

                uint32_t draw_count = batch->draw_indirect_buffer_view.size / sizeof(DrawIndexedIndirectCommand);
                command_buffer->draw_indexed_indirect(batch->draw_indirect_buffer_view.buffer, batch->draw_indirect_buffer_view.offset, draw_count, sizeof(DrawIndexedIndirectCommand));
            } else {
                ASSERT_MSG(0, "Draw mode not defined");
            }
        };

        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        shader->bind(command_buffer);

        UniformSetID uniform_sets[] = {
            renderer->vt_per_frame_uniform_set,
            renderer->transform_set,
        };
        command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

        std::vector<RenderBatch> &render_batches = renderer->main_render_batches;
        if (render_batches.size() > 0) {
            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
                    continue;
                for (auto &mesh_batch : batch.meshes)
                    DrawBatch(command_buffer, &mesh_batch, shader, 3);
            }
        }

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DepthPrePass::~DepthPrePass() {
    }
} // namespace mirai