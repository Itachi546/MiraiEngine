#include "DeferredPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass"), shader(nullptr), mesh_instance_set(K_INVALID_ID) {
        shader = nullptr;
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        shader = ShaderManager::get()->get_shader("gbuffer_pass");

        // Mesh Instance Data (Transform/Material)
        UniformLayout mesh_instance_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };
        mesh_instance_set = device->create_uniform_set(mesh_instance_layout, (uint32_t)std::size(mesh_instance_layout), 3, "mesh_instance_set");
        UniformBinding per_shader_bindings[] = {
            {.resource_id = renderer->transform_buffer, .buffer_info{.offset = 0}},
            {.resource_id = renderer->material_buffer, .buffer_info{.offset = 0}},
        };
        device->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));
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
            UniformSetID uniform_sets[] = {renderer->per_frame_uniform_set, mesh_instance_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, &node->renderpass_info);

            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};
            PipelineID pipeline_id = shader->get_pipeline_id();

            for (auto &batch : render_batches) {
                if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
                    continue;

                command_buffer->set_index_buffer(batch.index_buffer);
                command_buffer->set_uniform_sets(pipeline_id, &batch.vertex_binding_set, 1);
                for (uint32_t i = 0; i < batch.entities.size(); ++i) {
                    instance_data[0] = batch.entities[i];
                    instance_data[1] = batch.material_indices[i];
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