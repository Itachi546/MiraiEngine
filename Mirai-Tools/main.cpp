#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Graphics/Renderer.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Utils/FirstPersonController.hpp"

#include "ImGuiService.hpp"
#include "MainPass.hpp"

using namespace mirai;

class MainApplication : public App {
  public:
    MainApplication() : App("MiraiTools") {
    }

    void start() override {
        Window::get()->set_title("Mirai Tools");
        ImGuiService::Initialize();
        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
        frame_graph->load_from_file("Assets/mirai-tools-framegraph.json");
        frame_graph->set_renderer("main_pass", std::make_shared<MainPass>());
        frame_graph->compile();
    }

    void update() override {
        ImGuiService::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Options")) {
                ImGui::MenuItem("Open HDRI");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
    }

    ~MainApplication() {
        ImGuiService::Shutdown();
    }

  private:
};

int main() {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
    };

    auto engine = std::make_unique<Engine>(options);
    engine->set_app(std::make_unique<MainApplication>());

    engine->run();

    return 0;
}