#include "DeferredLightingPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {
    DeferredLightingPass::DeferredLightingPass() : FrameGraphRenderer("deferred_lighting_pass"), shader(nullptr), uniform_set(K_INVALID_ID) {
    }

    void DeferredLightingPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {

        shader = ShaderManager::get()->get_shader("pbr_deferred");
        rt_shader = ShaderManager::get()->get_shader("pbr_deferred_rt");

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
            {.resource_id = frame_graph->get_resource("cascaded_shadow_map")->handle, .texture_info = {.sampler = depth_sampler}},
            {.resource_id = frame_graph->get_resource("ssao_texture")->handle, .texture_info = {.sampler = default_sampler}},
        };

        // Create normal uniform set
        uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "deferred_binding_set");
        device->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

        // Create raytraced uniform set
        // For binding acceleration structure, we can only specify the descriptor type without actual
        // resources. The acceleration structure for now is single global entity and a type of ACCELRATION_STRUCTURE
        // is enough to distinguish it
        rt_uniform_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "deferred_rt_binding_set");
        bindings[4].resource_id = frame_graph->get_resource("rt_directional_shadow_map")->handle;
        desc.address_mode_u = desc.address_mode_v = desc.address_mode_w = SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerID no_repeat_sampler = device->create_sampler(&desc);
        bindings[4].texture_info = {.sampler = no_repeat_sampler};
        device->update_uniform_set(rt_uniform_set, bindings, cast_u32(std::size(bindings)));

        // Create cascade uniform set
        UniformLayout cascade_data = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        cascade_uniform_set = device->create_uniform_set(&cascade_data, 1, 2, "cascade_info_set");
        UniformBinding cascade_binding = {.resource_id = renderer->cascade_uniform_buffer};
        device->update_uniform_set(cascade_uniform_set, &cascade_binding, 1);
    }

    void DeferredLightingPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);

        ScopedGpuProfiling(command_buffer, "Deferred Lighting");

        ShaderMaterial *active_shader = renderer->enable_rt_shadow ? rt_shader : shader;

        device->begin_debug_utils_label(command_buffer, "DeferredLightingPass", nullptr);

        command_buffer->begin_render_pass(node, frame_graph);
        Scene *scene = renderer->get_scene();
        // @TODO Fix this
        PushConstant push_constant = {.data = nullptr, .shader_stage = SHADER_STAGE_FRAGMENT, .size = 0, .offset = 0};

        active_shader->bind(command_buffer, &node->renderpass_info);

        PipelineID pipeline_id = active_shader->get_pipeline_id();
        if (active_shader == shader) {
            UniformSetID uniform_sets[] = {uniform_set, cascade_uniform_set};
            command_buffer->set_uniform_sets(pipeline_id, uniform_sets, static_cast<uint32_t>(std::size(uniform_sets)));
        } else if (active_shader == rt_shader) {
            command_buffer->set_uniform_sets(pipeline_id, &rt_uniform_set, 1);
        }
        command_buffer->set_push_constants(pipeline_id, &push_constant, 1);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }

    DeferredLightingPass::~DeferredLightingPass() {
    }
} // namespace mirai