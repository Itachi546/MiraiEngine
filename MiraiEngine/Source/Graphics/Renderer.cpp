#include "Renderer.hpp"

#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterialCache.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "TextRenderManager.hpp"
#include "Common/Font.hpp"
#include "Common/MathUtils.hpp"
#include "Engine/Timer.hpp"

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer(bool enable_validation) : render_time_ms(16.0f) {
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>(enable_validation);
        scene = std::make_unique<Scene>("default");
        material_cache = std::make_unique<ShaderMaterialCache>();
        texture_cache = std::make_unique<TextureCache>();

        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());

        text_render_manager = std::make_unique<TextRenderManager>();

        default_font = LoadFont("Arial");
        text_render_manager->get_renderer_by_font(default_font.get());
    }

    void Renderer::compile_passes() {
    }

    void Renderer::update() {
        scene->update();

        if (Engine::get()->show_metrics) {
            TextRenderer *renderer = TextRenderManager::get()->get_default();
            renderer->AddText("Render time: " + utils::precision(render_time_ms, 2) + "ms", glm::vec2(10.0f, 20.0f), 12);

            uint32_t memory_usage = (uint32_t)utils::bytes_to_mb(device->get_memory_usage());
            renderer->AddText("GPU Memory Usage: " + std::to_string(memory_usage) + "MB", glm::vec2(10.0f, 34.0f), 12);
        }
    }

    void Renderer::render() {
        Timer timer;
        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        frame_graph->render(cb, scene.get());

        device->queue_command_buffer(cb);

        device->present();

        render_time_ms = (float)timer.elapsed_milliseconds();
    }

    Renderer::~Renderer() {
        device->wait();
        scene.reset();
    }

} // namespace mirai