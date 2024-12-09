#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterialCache.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "TextRenderManager.hpp"
#include "LineRenderer.hpp"
#include "Common/Font.hpp"
#include "Engine/Profiler.hpp"
#include "Math/MathUtils.hpp"
#include "Device/Window.hpp"

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer() {
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        material_cache = std::make_unique<ShaderMaterialCache>();
        texture_cache = std::make_unique<TextureCache>();
        miProfiler::Initialize();

        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());

        text_render_manager = std::make_unique<TextRenderManager>();

        default_font = LoadFont("Georgia");
        text_render_manager->get_renderer_by_font(default_font.get());

        line_renderer = std::make_unique<LineRenderer>();
    }

    void Renderer::compile_passes() {
    }

    void Renderer::update() {
        line_renderer->NewFrame();
        scene->update();
        std::stringstream ss("");
        ss << "Memory: " << std::fixed << std::setprecision(2) << utils::bytes_to_mb(device->get_memory_usage()) << "MB";
        TextRenderManager::get()->get_default()->AddText(ss.str(), glm::vec2{5.0f, 20.0f}, 14);
    }

    void Renderer::render() {

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();
        cb->begin();
        miProfiler::NewFrame(cb);
        {
            ScopedGpuProfiling(cb, "Gpu Time");

            frame_graph->render(cb, scene.get());

            device->queue_command_buffer(cb);
        }

        device->present();
        device->wait();
    }

    Renderer::~Renderer() {
        device->wait();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai