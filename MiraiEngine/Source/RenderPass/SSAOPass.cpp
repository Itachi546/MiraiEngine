#include "SSAOPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    void SSAOPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_unique<ComputeShader>("hbao_shader");
        shader->create_from_file("SPIRV/hbao.comp.spv");

        UniformLayout layouts[] = {
            {0, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
        };
        uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "ssao_uniform_set");

        ASSERT(node->inputs.size() == 1);
        ASSERT(node->outputs.size() == 1);

        TextureID depth_texture = frame_graph->get_resource(node->inputs[0])->handle;
        UniformBinding bindings[] = {
            {.resource_id = node->inputs[0]},
            {.resource_id = node->outputs[0]},
        };
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));
        shader->set_uniform_sets(&uniform_set, 1);

        push_constants.width = cast_float(node->width);
        push_constants.height = cast_float(node->height);
        push_constants.direction_step = 4.0f;
        push_constants.num_step = 8.0f;
        push_constants.radius = 1.0f;
        push_constants.step_size = 0.004f;
    }

    void SSAOPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ScopedGpuProfiling(command_buffer, "SSAO Pass");
        device->begin_debug_utils_label(command_buffer, "SSAO Pass", nullptr);

        PushConstant push_constants = {
            .data = &push_constants,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(PushConstants),
            .offset = 0,
        };

        shader->set_push_constant(&push_constants, 1);
        shader->bind(command_buffer);

        float ssao_push_constant = {};
        uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 1);

        device->end_debug_utils_label(command_buffer);
    }

    SSAOPass::~SSAOPass() {
    }

} // namespace mirai