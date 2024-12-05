#include "DepthPrePass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {
    DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass"), shader(nullptr), transform_set(K_INVALID_ID) {
    }

    void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {

        shader = std::make_shared<ShaderMaterial>("DepthPrePass");
        shader->create_from_file({
            "SPIRV/depth_prepass.vert.spv",
        });
        shader->set_depth_write(true);
        shader->set_depth_test(true);

        // Mesh Data
        UniformLayout transform_layout[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        transform_set = device->create_uniform_set(transform_layout, 1, 1, "depth_prepass_uniform_set");
    }

    void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        auto draw_batch = [&](DrawData *batches, uint32_t count, ShaderMaterial *shader) {
            // Set Per Frame Data
            UniformSetID uniform_sets[] = {scene->per_frame_uniform_set, transform_set};
            shader->set_uniform_sets(uniform_sets, (uint32_t)std::size(uniform_sets));
            shader->bind(command_buffer, &node->renderpass_info);

            uint32_t push_constant_data[4] = {0, 0, 0, 0};
            PushConstant push_constant = {.data = push_constant_data, .shader_stage = SHADER_STAGE_VERTEX, .size = sizeof(uint32_t) * 4, .offset = 0};

            PipelineID pipeline_id = shader->get_pipeline_id();
            uint32_t last_buffer_id = K_INVALID_ID;

            for (uint32_t i = 0; i < count; ++i) {
                BufferID current_buffer = batches[i].vertex_buffer;
                if (current_buffer.id != last_buffer_id) {
                    command_buffer->set_index_buffer(batches[i].index_buffer);
                    last_buffer_id = current_buffer.id;
                    command_buffer->set_uniform_sets(pipeline_id, &batches[i].vertex_binding_set, 1);
                }

                push_constant_data[0] = batches[i].transform_index;
                command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
                command_buffer->draw_indexed(batches[i].index_count,
                                             1,
                                             batches[i].index_offset,
                                             batches[i].vertex_offset,
                                             0);
            }
        };

        ScopedGpuProfiling(command_buffer, "DepthPrePass");

        device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

        UniformBinding per_shader_bindings[] = {
            {.resource_id = scene->transform_buffer, .offset = 0},
        };

        device->update_uniform_set(transform_set, per_shader_bindings, (uint32_t)std::size(per_shader_bindings));

        command_buffer->begin_render_pass(node, frame_graph);

        std::vector<DrawData> &opaque_batches = scene->opaque_batches;

        if (opaque_batches.size() > 0)
            draw_batch(opaque_batches.data(), static_cast<uint32_t>(opaque_batches.size()), shader.get());

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    DepthPrePass::~DepthPrePass() {
        shader = nullptr;
    }
} // namespace mirai