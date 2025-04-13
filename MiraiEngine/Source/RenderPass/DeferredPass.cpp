#include "DeferredPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    DeferredPass::DeferredPass() : FrameGraphRenderer("deferred_pass"), shader(nullptr), mesh_instance_set(K_INVALID_ID) {
        shader = nullptr;
    }

    void DeferredPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_shared<ShaderMaterial>("GBufferMaterial");
        shader->create_from_file({
            "SPIRV/gbuffer.vert.spv",
            "SPIRV/gbuffer.frag.spv",
        });

        shader->set_depth_write(true);
        shader->set_depth_test(true);

        // Mesh Instance Data (Transform/Material)
        UniformLayout mesh_instance_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };

        mesh_instance_set = device->create_uniform_set(mesh_instance_layout, (uint32_t)std::size(mesh_instance_layout), 3, "mesh_instance_set");
    }

    void DeferredPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);
        ScopedCpuProfiling("Deferred Render");
        ScopedGpuProfiling(command_buffer, "Deferred Pass");

        device->begin_debug_utils_label(command_buffer, "Deferred Pass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<RenderBatch> &render_batches = scene->main_render_batches;

        if (render_batches.size() > 0) {
            UniformBinding per_shader_bindings[] = {
                {.resource_id = scene->transform_buffer, .buffer_info{.offset = 0}},
                {.resource_id = scene->material_buffer, .buffer_info{.offset = 0}},
            };
            device->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));

            // Set Per Frame Data
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, mesh_instance_set};
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
                for (uint32_t i = 0; i < batch.transform_indices.size(); ++i) {
                    instance_data[0] = batch.transform_indices[i];
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
        if (shader)
            shader = nullptr;
    }
} // namespace mirai