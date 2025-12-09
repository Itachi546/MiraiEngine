#include "DebugPass.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/TextRenderManager.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Device/Window.hpp"
#include "Common/Font.hpp"
namespace mirai {

    DebugPass::DebugPass() : FrameGraphRenderer("debug_pass") {
    }

    void DebugPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        text_render_manager = std::make_unique<TextRenderManager>();
        default_font = LoadFont("Georgia");
        text_render_manager->get_renderer_by_font(default_font.get());
    }

    void DebugPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {

        TextRenderManager *text_render_manager = TextRenderManager::get();
        if (text_render_manager->renderers.size() == 0)
            return;

        bool has_drawable = false;
        for (auto &renderer : text_render_manager->renderers) {
            if (renderer->vertex_count > 0) {
                has_drawable = true;
                break;
            }
        }

        if (!has_drawable)
            return;

        device->begin_debug_utils_label(command_buffer, "Text Rendering", nullptr);
        ScopedGpuProfiling(command_buffer, "Debug Draw");

        command_buffer->begin_render_pass(node, frame_graph);

        text_render_manager->shader->bind(command_buffer, &node->renderpass_info);
        PipelineID pipeline = text_render_manager->shader->get_pipeline_id();

        struct PushConstantData {
            glm::mat4 ortho_matrix;
            uint32_t texture_id;
            uint32_t _unused[3];
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

        // miProfiler::DrawData();
        for (auto &renderer : text_render_manager->renderers) {
            if (renderer->vertex_count == 0)
                continue;
            push_constant_data.texture_id = renderer->get_font_texture().id;

            command_buffer->set_uniform_sets(pipeline, &renderer->uniform_set, 1);
            command_buffer->set_push_constants(pipeline, &push_constant, 1);
            command_buffer->draw(renderer->vertex_count, 1, 0, 0);

            renderer->reset();
        }

        command_buffer->end_render_pass();
    }
} // namespace mirai