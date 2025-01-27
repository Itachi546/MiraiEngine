#include "ForwardPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    ForwardPass::ForwardPass() : FrameGraphRenderer("forward_pass"), opaque_shader(nullptr), transparent_shader(nullptr), mesh_instance_set(K_INVALID_ID) {
    }

    void ForwardPass::initialize(FrameGraph *framegraph, const FrameGraphNode *node) {
        opaque_shader = std::make_shared<ShaderMaterial>("ForwardPassMaterial");
        opaque_shader->create_from_file({
            "SPIRV/forward_pass.vert.spv",
            "SPIRV/forward_pass.frag.spv",
        });
        opaque_shader->set_depth_write(false);
        opaque_shader->set_depth_test(true);
        opaque_shader->set_depth_compare_op(COMPARE_OP_EQUAL);

        transparent_shader = std::make_shared<ShaderMaterial>("TransparentMaterial");
        transparent_shader->create_from_file({
            "SPIRV/forward_pass.vert.spv",
            "SPIRV/transparent.frag.spv",
        });
        transparent_shader->set_enable_blend(true);
        transparent_shader->set_cull_mode(CULL_MODE_NONE);
        transparent_shader->set_depth_write(true);
        transparent_shader->set_depth_test(true);

        // Mesh Data
        UniformLayout mesh_data_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        // Mesh Instance Data (Transform/Material)
        UniformLayout mesh_instance_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };

        mesh_instance_set = device->create_uniform_set(mesh_instance_layout, (uint32_t)std::size(mesh_instance_layout), 3, "mesh_instance_set");
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        auto draw_batch = [&](DrawData *batches, uint32_t count, ShaderMaterial *shader) {
            // Set Per Frame Data
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, mesh_instance_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, &node->renderpass_info);

            uint32_t instance_data[] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = instance_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            PipelineID pipeline_id = shader->get_pipeline_id();
            uint32_t last_buffer_id = K_INVALID_ID;
            for (uint32_t i = 0; i < count; ++i) {
                BufferID current_buffer = batches[i].vertex_buffer;
                if (current_buffer.id != last_buffer_id) {
                    command_buffer->set_index_buffer(batches[i].index_buffer);
                    last_buffer_id = current_buffer.id;
                    command_buffer->set_uniform_sets(pipeline_id, &batches[i].vertex_binding_set, 1);
                }

                instance_data[0] = batches[i].transform_index;
                instance_data[1] = batches[i].material_index;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batches[i].index_count,
                                             1,
                                             batches[i].index_offset,
                                             batches[i].vertex_offset,
                                             0);
            }
        };

        ScopedGpuProfiling(command_buffer, "Forward Pass");

        device->begin_debug_utils_label(command_buffer, "ForwardPass", nullptr);

        // Update Per Pipeline Data (Transform/Material)
        UniformBinding per_shader_bindings[] = {
            {.resource_id = scene->transform_buffer, .offset_or_mip_level = 0},
            {.resource_id = scene->material_buffer, .offset_or_mip_level = 0},
        };
        device->update_uniform_set(mesh_instance_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &opaque_batches = scene->main_opaque_draw_batch;
        if (opaque_batches.size() > 0)
            draw_batch(opaque_batches.data(), static_cast<uint32_t>(opaque_batches.size()), opaque_shader.get());

        std::vector<DrawData> &transparent_batches = scene->main_transparent_draw_batch;
        if (transparent_batches.size() > 0)
            draw_batch(transparent_batches.data(), static_cast<uint32_t>(transparent_batches.size()), transparent_shader.get());

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    ForwardPass::~ForwardPass() {
        if (transparent_shader)
            transparent_shader = nullptr;
        if (opaque_shader)
            opaque_shader = nullptr;
    }
} // namespace mirai