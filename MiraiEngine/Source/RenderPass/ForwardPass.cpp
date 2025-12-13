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
        // Mesh Instance Data (Transform/Material)
        mesh_instance_layouts[0] = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};
        mesh_instance_layouts[1] = {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT};

        // Initialize PerFrame uniform set
        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT,
        };
        per_frame_uniform_set = device->create_uniform_set(&layout, 1, 0, "per_frame_uniform_set");

        UniformBinding binding = {
            .resource_id = renderer->per_frame_uniform_buffer,
            .buffer_info = {
                .offset = 0,
                .range = sizeof(Scene::FrameData),
            },
        };
        device->update_uniform_set(per_frame_uniform_set, &binding, 1);
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("FrameGraph::ForwardPass");

        auto draw_batch = [&](MeshBatch *batch, PipelineID pipeline_id) {
            if (batch->transform_indices.size() == 0)
                return;

            // Set Per Frame Data
            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            UniformSetID mesh_instance_set = command_buffer->create_uniform_set(mesh_instance_layouts, cast_u32(std::size(mesh_instance_layouts)), 3);
            UniformBinding per_shader_bindings[] = {
                {.resource_id = batch->transform_buffer_view.buffer, .buffer_info{.offset = batch->transform_buffer_view.offset, .range = batch->transform_buffer_view.size}},
                {.resource_id = batch->material_buffer_view.buffer, .buffer_info{.offset = batch->material_buffer_view.offset, .range = batch->material_buffer_view.size}},
            };
            device->update_uniform_set(mesh_instance_set, per_shader_bindings, cast_u32(std::size(per_shader_bindings)));

            UniformSetID uniform_sets[] = {batch->vertex_binding_set, mesh_instance_set};
            command_buffer->set_uniform_sets(pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
            command_buffer->set_index_buffer(batch->index_buffer.buffer);

            for (uint32_t i = 0; i < batch->transform_indices.size(); ++i) {
                instance_data[0] = i;
                instance_data[1] = i;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batch->index_counts[i],
                                             1,
                                             batch->index_offsets[i],
                                             batch->vertex_offsets[i],
                                             0);
            }
            device->destroy_uniform_sets(&mesh_instance_set, 1);
        };
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
                command_buffer->set_uniform_sets(shader->pipeline_id, &per_frame_uniform_set, 1);

                for (auto &mesh_batch : batch.meshes) {
                    draw_batch(&mesh_batch, shader->pipeline_id);
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
                command_buffer->set_uniform_sets(shader->pipeline_id, &per_frame_uniform_set, 1);
                for (auto &mesh_batch : batch.meshes) {
                    draw_batch(&mesh_batch, shader->pipeline_id);
                }
            }
        }
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    ForwardPass::~ForwardPass() {
    }
} // namespace mirai