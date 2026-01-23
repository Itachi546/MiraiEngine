#include "TAAResolvePass.hpp"
#include "Scene/Shader.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {

    void TAAResolvePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        UniformLayout layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
            {.binding = 1, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
            {.binding = 2, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
            {.binding = 3, .binding_type = BINDING_TYPE_STORAGE_IMAGE, .shader_stage = SHADER_STAGE_COMPUTE},
        };

        uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "taa_resolve_set");

        FrameGraphResource *gbuffer_lighting = frame_graph->get_resource("gbuffer_lighting");
        ASSERT(gbuffer_lighting != nullptr);

        FrameGraphResource *gbuffer_depth = frame_graph->get_resource("gbuffer_depth");
        ASSERT(gbuffer_depth != nullptr);

        FrameGraphResource *taa_history = frame_graph->get_resource("taa_history_buffer");
        ASSERT(taa_history != nullptr);

        FrameGraphResource *gbuffer_velocity = frame_graph->get_resource("gbuffer_velocity");
        ASSERT(gbuffer_velocity != nullptr);

        SamplerDescription sampler_desc = SamplerDescription::create();
        SamplerID sampler = device->create_sampler(&sampler_desc);

        sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_NEAREST;
        SamplerID depth_sampler = device->create_sampler(&sampler_desc);

        UniformBinding bindings[] = {
            {.resource_id = gbuffer_lighting->handle, .texture_info = {.sampler = sampler}},
            {.resource_id = gbuffer_depth->handle, .texture_info = {.sampler = depth_sampler}},
            {.resource_id = gbuffer_velocity->handle, .texture_info = {.sampler = sampler}},
            {.resource_id = taa_history->handle},
        };

        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

        shader = Shader::create_from_file("SPIRV/taa-resolve.comp.spv", "taa-resolve-shader");
        copy_texture_shader = Shader::create_from_file("SPIRV/copy-texture.comp.spv", "taa-copy-texture-shader");
    }

    void TAAResolvePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ScopedGpuProfiling(command_buffer, "TAAResolve");
        device->begin_debug_utils_label(command_buffer, "TAA Resolve", nullptr);

        FrameGraphResource *gbuffer_lighting = frame_graph->get_resource("gbuffer_lighting");
        ASSERT(gbuffer_lighting != nullptr);

        FrameGraphResource *taa_history = frame_graph->get_resource("taa_history_buffer");
        ASSERT(taa_history != nullptr);

        // Copy output texture to TAA history
        float push_constant_data[] = {
            1.0f / cast_float(gbuffer_lighting->resource_info.width),
            1.0f / cast_float(gbuffer_lighting->resource_info.height),
            0,
            0};

        PushConstant push_constants = {
            .data = push_constant_data,
            .offset = 0,
            .size = sizeof(float) * 4,
            .shader_stage = SHADER_STAGE_COMPUTE,
        };

        TextureBarrierInfo barrier_infos[] = {
            {
                .texture_id = gbuffer_lighting->handle,
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            },
            {
                .texture_id = taa_history->handle,
                .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_READ,
                .layout = IMAGE_LAYOUT_GENERAL,
            },
        };

        command_buffer->prepare_image(barrier_infos, first_frame ? 2 : 1);

        if (first_frame) {
            UniformLayout copy_texture_layouts[] = {
                {.binding = 0, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
                {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_IMAGE, .shader_stage = SHADER_STAGE_COMPUTE},
            };
            UniformSetID copy_texture_uniform_set = command_buffer->create_uniform_set(copy_texture_layouts, cast_u32(std::size(copy_texture_layouts)), 0);

            SamplerDescription sampler_desc = SamplerDescription::create();
            SamplerID sampler = device->create_sampler(&sampler_desc);

            UniformBinding copy_bindings[] = {
                {.resource_id = gbuffer_lighting->handle, .texture_info = {.sampler = sampler}},
                {.resource_id = taa_history->handle},
            };

            device->update_uniform_set(copy_texture_uniform_set, copy_bindings, cast_u32(std::size(copy_bindings)));

            copy_texture_shader->bind(command_buffer);
            command_buffer->set_uniform_sets(copy_texture_shader->pipeline_id, &copy_texture_uniform_set, 1);
            command_buffer->set_push_constants(copy_texture_shader->pipeline_id, &push_constants, 1);

            uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
            uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

            command_buffer->dispatch(work_size_x, work_size_y, 1);

        } else {

            shader->bind(command_buffer);
            command_buffer->set_uniform_sets(shader->pipeline_id, &uniform_set, 1);
            command_buffer->set_push_constants(shader->pipeline_id, &push_constants, 1);

            uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
            uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

            command_buffer->dispatch(work_size_x, work_size_y, 1);
        }

        device->end_debug_utils_label(command_buffer);
        first_frame = false;
    }
} // namespace mirai