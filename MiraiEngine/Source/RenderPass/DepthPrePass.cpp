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

        UniformLayout transform_material_layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };
        transform_set = device->create_uniform_set(transform_material_layouts, cast_u32(std::size(transform_material_layouts)), 1);

        UniformBinding bindings[] = {
            {.resource_id = renderer->global_transform_buffer},
        };
        device->update_uniform_set(transform_set, bindings, cast_u32(std::size(bindings)));
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        shader->bind(command_buffer);

        UniformSetID uniform_sets[] = {
            renderer->vt_per_frame_uniform_set,
            transform_set,
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