#include "SwapchainCopyPass.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/TextRenderManager.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"
#include "Scene/Shader.hpp"
#include "Graphics/Renderer.hpp"

#include <string>

namespace mirai {

    SwapchainCopyPass::SwapchainCopyPass() : FrameGraphRenderer("swapchain_copy"), enable_aa(true) {
    }

    void SwapchainCopyPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        PipelineState pipeline_state = {};
        pipeline_state.custom_shader_id = Shader::create_shader_id();
        PipelineAttachmentInfo attachment_info = {
            .color_attachments_format = {FORMAT_B8G8R8A8_UNORM},
            .has_depth_attachment = false,
        };

        shader = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/fullscreen.vert.spv", "SPIRV/swapchain-copy.frag.spv"}, "swapchain-copy-shader");
        if (shader == nullptr) {
            Log::Fatal("Failed to create swapchain copy pipeline");
        }

        FrameGraphResource *input_texture = frame_graph->get_resource(node->inputs[0]);

        UniformLayout bounded_uniform = {
            .binding = 0,
            .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        uniform_set = device->create_uniform_set(&bounded_uniform, 1, 0, "full_screen_input");

        SamplerDescription sampler_desc = SamplerDescription::create();
        SamplerID default_sampler = device->create_sampler(&sampler_desc);

        UniformBinding bindings = {.resource_id = input_texture->handle, .texture_info = {.sampler = default_sampler}};
        device->update_uniform_set(uniform_set, &bindings, 1);
    }

    void SwapchainCopyPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        ASSERT(node != nullptr);

        device->begin_debug_utils_label(command_buffer, "Swapchain + FXAA", nullptr);
        ScopedGpuProfiling(command_buffer, "FXAA");

        uint32_t width, height;
        Window::get()->get_size(&width, &height);
        node->width = width;
        node->height = height;

        float push_constant_data[4] = {(float)width, (float)height, static_cast<float>(enable_aa), 0.0f};
        PushConstant push_constants = {
            .data = push_constant_data,
            .shader_stage = SHADER_STAGE_FRAGMENT,
            .size = sizeof(float) * 4,
            .offset = 0,
        };

        command_buffer->begin_render_pass(node, frame_graph);

        shader->bind(command_buffer);
        command_buffer->set_uniform_sets(shader->pipeline_id, &uniform_set, 1);
        command_buffer->set_push_constants(shader->pipeline_id, &push_constants, 1);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    SwapchainCopyPass::~SwapchainCopyPass() {
    }

} // namespace mirai