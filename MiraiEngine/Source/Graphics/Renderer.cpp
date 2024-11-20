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
#include "Engine/Profiler.hpp"

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer(bool enable_validation) {
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
    }

    void Renderer::render() {
        ScopedCpuProfiling("Render Time");

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        frame_graph->render(cb, scene.get());

        device->queue_command_buffer(cb);

        device->present();
    }

    Renderer::~Renderer() {
        device->wait();
        scene.reset();
    }

} // namespace mirai