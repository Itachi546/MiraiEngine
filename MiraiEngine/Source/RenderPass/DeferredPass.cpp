/*
#include "DeferredPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass"), shader(nullptr) {
        shader = nullptr;
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        shader = ShaderManager::get()->get_shader("gbuffer_pass");

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

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("Deferred Render");
        ScopedGpuProfiling(command_buffer, "Deferred Pass");

        device->begin_debug_utils_label(command_buffer, "Deferred Pass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        Scene *scene = renderer->get_scene();
        std::vector<RenderBatch> &render_batches = scene->main_render_batches;

        if (render_batches.size() > 0) {
            // Set Per Frame Data
            PipelineID pipeline_id = shader->get_pipeline_id();
            shader->bind(command_buffer, &node->renderpass_info);
            command_buffer->set_uniform_sets(pipeline_id, &per_frame_uniform_set, 1);

            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
                    continue;

                UniformSetID mesh_instance_set = command_buffer->create_uniform_set(mesh_instance_layouts, (uint32_t)std::size(mesh_instance_layouts), 3);
                UniformBinding per_shader_bindings[] = {
                    {.resource_id = batch.transform_buffer_view.buffer, .buffer_info{.offset = batch.transform_buffer_view.offset, .range = batch.transform_buffer_view.size}},
                    {.resource_id = batch.material_buffer_view.buffer, .buffer_info{.offset = batch.material_buffer_view.offset, .range = batch.material_buffer_view.size}},
                };
                device->update_uniform_set(mesh_instance_set, per_shader_bindings, 2);

                UniformSetID uniform_sets[] = {
                    mesh_instance_set,
                    batch.vertex_binding_set,
                };
                command_buffer->set_uniform_sets(pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

                command_buffer->set_index_buffer(batch.index_buffer);
                for (uint32_t i = 0; i < batch.transform_indices.size(); ++i) {
                    instance_data[0] = i;
                    instance_data[1] = i;
                    command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                    command_buffer->draw_indexed(batch.index_counts[i],
                                                 1,
                                                 batch.index_offsets[i],
                                                 batch.vertex_offsets[i],
                                                 0);
                }
            }
        }
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DeferredPass::~DeferredPass() {
    }
} // namespace mirai
*/