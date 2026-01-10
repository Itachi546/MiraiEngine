#include "DeferredLightingPass.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {
    DeferredLightingPass::DeferredLightingPass() : FrameGraphRenderer("deferred_lighting_pass"), cascade_shader(nullptr) {
    }

    void DeferredLightingPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        PipelineAttachmentInfo attachment_info = {
            .color_attachments_format = {FORMAT_R16G16B16A16_SFLOAT},
            .has_depth_attachment = false,
        };

        PipelineState pipeline_state = {};
        pipeline_state.custom_shader_id = Shader::create_shader_id();
        cascade_shader = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred-lighting.frag.spv"}, "deferred-lighting-shader");

        pipeline_state.custom_shader_id = Shader::create_shader_id();
        rt_shader = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred-lighting-rt.frag.spv"}, "deferred-lighting-shader-rt");

        // Deferred Shading Textures
        uint32_t binding_count = static_cast<uint32_t>(node->inputs.size());

        UniformLayout layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 1, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 2, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 3, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 4, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
            {.binding = 5, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };

        SamplerDescription desc = SamplerDescription::create();
        SamplerID default_sampler = device->create_sampler(&desc);

        desc.min_filter = desc.mag_filter = FILTER_NEAREST;
        SamplerID depth_sampler = device->create_sampler(&desc);

        UniformBinding bindings[] = {
            {.resource_id = frame_graph->get_resource("gbuffer_color")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_depth")->handle, .texture_info = {.sampler = depth_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_normal")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("gbuffer_emissive")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = frame_graph->get_resource("ssao_texture")->handle, .texture_info = {.sampler = default_sampler}},
            {.resource_id = /*frame_graph->get_resource("cascaded_shadow_map")->handle*/ {K_INVALID_ID}, .texture_info = {.sampler = depth_sampler}},
        };

        // Create normal uniform set
        // cascaded_shadow_uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "deferred_binding_set");
        // device->update_uniform_set(cascaded_shadow_uniform_set, bindings, cast_u32(std::size(bindings)));

        // Create raytraced uniform set
        // For binding acceleration structure, we can only specify the descriptor type without actual
        // resources. The acceleration structure for now is single global entity and a type of ACCELRATION_STRUCTURE
        // is enough to distinguish it
        rt_shadow_uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "deferred_rt_binding_set");
        bindings[5].resource_id = frame_graph->get_resource("rt_directional_shadow_map")->handle;
        desc.address_mode_u = desc.address_mode_v = desc.address_mode_w = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerID no_repeat_sampler = device->create_sampler(&desc);
        bindings[5].texture_info = {.sampler = no_repeat_sampler};
        device->update_uniform_set(rt_shadow_uniform_set, bindings, cast_u32(std::size(bindings)));
    }

    void DeferredLightingPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);

        ScopedGpuProfiling(command_buffer, "Deferred Lighting");

        Shader *active_shader = renderer->enable_rt_shadow ? rt_shader : cascade_shader;

        device->begin_debug_utils_label(command_buffer, "DeferredLightingPass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);
        Scene *scene = renderer->get_scene();

        active_shader->bind(command_buffer);

        // Create cascade/per-frame uniform set
        UniformLayout layouts[] = {
            {
                .binding = 0,
                .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
                .shader_stage = SHADER_STAGE_FRAGMENT,
            },
            {
                .binding = 1,
                .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
                .shader_stage = SHADER_STAGE_FRAGMENT,

            },
        };

        uint32_t binding_count = active_shader == rt_shader ? 1 : 2;

        UniformSetID per_frame_uniform_set = command_buffer->create_uniform_set(layouts, binding_count, 2);
        UniformBinding bindings[] = {
            {
                .resource_id = renderer->per_frame_uniform_buffer.buffer,
                .buffer_info = {
                    .offset = renderer->per_frame_uniform_buffer.offset,
                    .range = renderer->per_frame_uniform_buffer.size,
                },
            },
            {
                .resource_id = renderer->cascade_uniform_buffer.buffer,
                .buffer_info = {
                    .offset = renderer->cascade_uniform_buffer.offset,
                    .range = renderer->cascade_uniform_buffer.size,
                },
            },
        };

        device->update_uniform_set(per_frame_uniform_set, bindings, binding_count);

        PipelineID pipeline_id = active_shader->pipeline_id;
        UniformSetID uniform_sets[] = {per_frame_uniform_set, active_shader == rt_shader ? rt_shadow_uniform_set : cascaded_shadow_uniform_set};
        command_buffer->set_uniform_sets(pipeline_id, uniform_sets, static_cast<uint32_t>(std::size(uniform_sets)));

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }

    DeferredLightingPass::~DeferredLightingPass() {
    }
} // namespace mirai