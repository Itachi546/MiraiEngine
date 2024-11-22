#include "SwapchainCopyPass.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/TextRenderManager.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Engine/Profiler.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <string>

namespace mirai {

    SwapchainCopyPass::SwapchainCopyPass() : FrameGraphRenderPass("swapchain_copy"), enable_aa(true) {
        material = std::make_shared<ShaderMaterial>("FullScreenTextureMaterial");
        material->create_from_file(std::vector<std::string>{
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/fullscreen.frag.spv",
        });
        material->set_cull_mode(CULL_MODE_BACK);
    }

    void SwapchainCopyPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        FrameGraphResource *input_texture = frame_graph->get_resource(node->inputs[0]);

        UniformLayout bounded_uniform = {
            .binding = 0,
            .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        uniform_set = RenderingDevice::get()->create_uniform_set(&bounded_uniform, 1, 0, "full_screen_input");

        UniformBinding bindings = {.resource_id = input_texture->texture};
        RenderingDevice::get()->update_uniform_set(uniform_set, &bindings, 1);
        material->set_uniform_sets(&uniform_set, 1);
    }

    void SwapchainCopyPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        ASSERT(node != nullptr);

        RenderingDevice::get()->begin_debug_utils_label(command_buffer, "Swapchain + FXAA", nullptr);

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

        material->bind(command_buffer, node, frame_graph);

        command_buffer->draw(6, 1, 0, 0);

        // Draw Text
        render_text(command_buffer, frame_graph, node);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }

    SwapchainCopyPass::~SwapchainCopyPass() {
    }

    void SwapchainCopyPass::render_text(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node) {
        TextRenderManager *text_render_manager = TextRenderManager::get();
        if (text_render_manager->renderers.size() == 0)
            return;

        text_render_manager->shader->bind(command_buffer, node, frame_graph);
        PipelineID pipeline = text_render_manager->shader->get_pipeline_id();

        struct PushConstantData {
            glm::mat4 ortho_matrix;
            uint32_t texture_id;
        } push_constant_data;

        uint32_t width, height;
        Window::get()->get_size(&width, &height);
        push_constant_data.ortho_matrix = glm::ortho(0.0f, (float)width, 0.0f, (float)height);

        PushConstant push_constant = {
            .data = &push_constant_data,
            .shader_stage = SHADER_STAGE_VERTEX,
            .size = sizeof(PushConstantData),
            .offset = 0,
        };

        miProfiler::DrawData();
        for (auto &renderer : text_render_manager->renderers) {
            if (renderer->vertex_count == 0)
                continue;
            push_constant_data.texture_id = renderer->get_font_texture().id;

            command_buffer->set_uniform_sets(pipeline, &renderer->uniform_set, 1);
            command_buffer->set_push_constants(pipeline, &push_constant, 1);
            command_buffer->draw(renderer->vertex_count, 1, 0, 0);

            renderer->reset();
        }
    }

} // namespace mirai