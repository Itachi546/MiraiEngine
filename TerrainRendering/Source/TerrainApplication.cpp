#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "TerrainPass.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Graphics/Renderer.hpp"
#include "RenderPass/RenderPass.hpp"

#include <memory>

using namespace mirai;

class TerrainApplication : public App {
  public:
    TerrainApplication() : App("TerrainApplication") {
        Window::get()->set_title("TerrainApplication");
    }

    void start() override {
        Scene *scene = Renderer::get()->get_scene();
        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/daytime.hdr");
        scene->set_environment_map(env_map);

        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
        frame_graph->load_from_file("Assets/terrain_pass.json");
        frame_graph->set_renderer("terrain_pass", std::make_shared<TerrainPass>(512, 512, 6));
        frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
        frame_graph->set_renderer("sky_pass", std::make_shared<Overlay3DPass>());
    }

    void update() override {
    }

    ~TerrainApplication() {
    }

  private:
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
    };

    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    engine->set_app(std::make_unique<TerrainApplication>());
    engine->run();
    engine = nullptr;
}