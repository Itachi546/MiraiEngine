#include "SSAOPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"

namespace mirai {

    void SSAOPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        shader = std::make_unique<ComputeShader>("hbao_shader");
        shader->create_from_file("SPIRV/hbao.comp.spv");

        noise_texture = rendering_utils::load_texture2d_from_path("Assets/Textures/noise.png");

        UniformLayout layouts[] = {
            {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {2, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
        };
        uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "ssao_uniform_set");

        ASSERT(node->inputs.size() == 1);
        ASSERT(node->outputs.size() == 1);

        TextureID depth_texture = frame_graph->get_resource(node->inputs[0])->handle;
        TextureID ssao_texture = frame_graph->get_resource(node->outputs[0])->handle;
        UniformBinding bindings[] = {
            {.resource_id = ssao_texture},
            {.resource_id = depth_texture},
            {.resource_id = noise_texture},
        };
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));
        shader->set_uniform_sets(&uniform_set, 1);

        constant_data.width = cast_float(node->width);
        constant_data.height = cast_float(node->height);
        constant_data.direction_step = 4.0f;
        constant_data.num_step = 8.0f;
        constant_data.radius = 1.0f;
        constant_data.step_size = 0.004f;
    }

    void SSAOPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ScopedGpuProfiling(command_buffer, "SSAO Pass");
        device->begin_debug_utils_label(command_buffer, "SSAO Pass", nullptr);

        constant_data.inv_projection_matrix = scene->get_camera()->get_inv_projection_transform();
        PushConstant push_constants = {
            .data = &constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(PushConstants),
            .offset = 0,
        };

        command_buffer->begin_compute_pass(node, frame_graph);
        shader->set_push_constant(&push_constants, 1);
        shader->bind(command_buffer);

        float ssao_push_constant = {};
        uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 1);

        device->end_debug_utils_label(command_buffer);
    }

    SSAOPass::~SSAOPass() {
        device->destroy_textures(&noise_texture, 1);
    }

} // namespace mirai