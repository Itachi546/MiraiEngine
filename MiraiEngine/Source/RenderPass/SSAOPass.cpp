#include "SSAOPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"

namespace mirai {

    void SSAOPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        ssao_shader = std::make_unique<ComputeShader>("hbao_shader");
        ssao_shader->create_from_file("SPIRV/hbao.comp.spv");
        blur_shader = std::make_unique<ComputeShader>("cross-bilateral-blur-shader");
        blur_shader->create_from_file("SPIRV/cross-bilateral-blur.comp.spv");

        noise_texture = rendering_utils::load_texture2d_from_path("Assets/Textures/noise.png");

        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = node->width,
            .height = node->height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_R16_SFLOAT,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
        };

        blur_intermediate_texture = device->create_texture(&texture_desc, "blur_intermediate_texture");

        ASSERT(node->inputs.size() == 1);
        ASSERT(node->outputs.size() == 1);
        TextureID depth_texture = frame_graph->get_resource(node->inputs[0])->handle;
        TextureID ssao_texture = frame_graph->get_resource(node->outputs[0])->handle;

        UniformLayout layouts[] = {
            {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
            {2, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
        };

        SamplerDescription sampler_desc = SamplerDescription::create();
        SamplerID default_sampler = device->create_sampler(&sampler_desc);

        sampler_desc.address_mode_u = sampler_desc.address_mode_v = sampler_desc.address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT;
        SamplerID noise_sampler = device->create_sampler(&sampler_desc);

        UniformBinding bindings[] = {
            {.resource_id = ssao_texture},
            {.resource_id = depth_texture, .texture_info = {.sampler = default_sampler}},
            {.resource_id = noise_texture, .texture_info = {.sampler = noise_sampler}},
        };
        // SSAO Uniform Set
        ssao_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "ssao_uniform_set");
        device->update_uniform_set(ssao_set, bindings, cast_u32(std::size(bindings)));
        ssao_shader->set_uniform_sets(&ssao_set, 1);

        // Create BlurX Uniform Set
        blur_x_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "blur_x_set");
        bindings[0].resource_id = blur_intermediate_texture;
        bindings[2].resource_id = ssao_texture;
        device->update_uniform_set(blur_x_set, bindings, cast_u32(std::size(bindings)));

        // Create BlurY Uniform Set
        blur_y_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "blur_y_set");
        bindings[0].resource_id = ssao_texture;
        bindings[2].resource_id = blur_intermediate_texture;
        device->update_uniform_set(blur_y_set, bindings, cast_u32(std::size(bindings)));

        constant_data.width = cast_float(node->width);
        constant_data.height = cast_float(node->height);
        constant_data.direction_step = 4.0f;
        constant_data.num_step = 8.0f;
        constant_data.radius = 1.0f;
        constant_data.step_size = 0.005f;
        constant_data.intensity = 1.5f;
        constant_data.tangent_bias = 0.6f;
    }

    void SSAOPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ScopedCpuProfiling("SSAO Update");
        ScopedGpuProfiling(command_buffer, "SSAO Pass");
        device->begin_debug_utils_label(command_buffer, "SSAO Pass", nullptr);

        ssao_pass(command_buffer, frame_graph, node, scene);

        TextureID ssao_texture = frame_graph->get_resource(node->outputs[0])->handle;
        TextureBarrierInfo barrier_infos[] = {
            {
                .texture_id = ssao_texture,
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .texture_id = blur_intermediate_texture,
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_WRITE,
                .layout = IMAGE_LAYOUT_GENERAL,
            },
        };
        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        // Blur in x-direction
        ssao_blur(command_buffer, frame_graph, node, scene, 0);

        barrier_infos[0].texture_id = blur_intermediate_texture;
        barrier_infos[1].texture_id = ssao_texture;
        command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

        // Blur in y-direction
        ssao_blur(command_buffer, frame_graph, node, scene, 1);
        device->end_debug_utils_label(command_buffer);
    }

    void SSAOPass::ssao_pass(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        constant_data.inv_projection_matrix = scene->get_camera()->get_inv_projection_transform();
        PushConstant push_constants = {
            .data = &constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(PushConstants),
            .offset = 0,
        };

        command_buffer->begin_compute_pass(node, frame_graph);
        ssao_shader->set_push_constant(&push_constants, 1);
        ssao_shader->bind(command_buffer);

        float ssao_push_constant = {};
        uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 1);
    }

    void SSAOPass::ssao_blur(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene, float direction) {
        Camera *camera = scene->get_camera();

        float push_constant_data[] = {cast_float(node->width), cast_float(node->height),
                                      direction, blur_radius,
                                      blur_sharpness,
                                      camera->get_near_plane(), camera->get_far_plane()};

        PushConstant push_constants = {
            .data = &push_constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(float) * std::size(push_constant_data),
            .offset = 0,
        };

        UniformSetID uniform_set = direction == 0 ? blur_x_set : blur_y_set;
        blur_shader->set_push_constant(&push_constants, 1);
        blur_shader->set_uniform_sets(&uniform_set, 1);
        blur_shader->bind(command_buffer);

        float ssao_push_constant = {};
        uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
        uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

        command_buffer->dispatch(work_size_x, work_size_y, 1);
    }

    SSAOPass::~SSAOPass() {
        if (noise_texture.is_valid())
            device->destroy_textures(&noise_texture, 1);
        if (blur_intermediate_texture.is_valid())
            device->destroy_textures(&blur_intermediate_texture, 1);
    }

} // namespace mirai