#include "SwapchainCopyPass.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/TextRenderManager.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"

#include <string>

namespace mirai {

    SwapchainCopyPass::SwapchainCopyPass() : FrameGraphRenderer("swapchain_copy"), enable_aa(true), material(nullptr) {
    }

    void SwapchainCopyPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        material = std::make_shared<ShaderMaterial>("FullScreenTextureMaterial");
        material->create_from_file(std::vector<std::string>{
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/fullscreen.frag.spv",
        });
        material->set_cull_mode(CULL_MODE_BACK);

        FrameGraphResource *input_texture = frame_graph->get_resource(node->inputs[0]);

        UniformLayout bounded_uniform = {
            .binding = 0,
            .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        uniform_set = device->create_uniform_set(&bounded_uniform, 1, 0, "full_screen_input");

        UniformBinding bindings = {.resource_id = input_texture->handle};
        device->update_uniform_set(uniform_set, &bindings, 1);
        material->set_uniform_sets(&uniform_set, 1);
    }

    void SwapchainCopyPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
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
        material->set_push_constant(&push_constants, 1);

        command_buffer->begin_render_pass(node, frame_graph);

        material->bind(command_buffer, &node->renderpass_info);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    SwapchainCopyPass::~SwapchainCopyPass() {
        material = nullptr;
    }

} // namespace mirai