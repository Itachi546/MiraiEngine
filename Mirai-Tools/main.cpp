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
#include "HDRIConverterUI.hpp"
#include "HDRIConverterPass.hpp"

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
        hdri_pass = std::make_shared<HDRIConverterPass>(1024, 1024);
        frame_graph->set_renderer("cubemap_gen_pass", hdri_pass);

        frame_graph->compile();

        hdri_ui = std::make_unique<HDRIConverterUI>(hdri_pass);
        hdri_ui->set_texture("C:/Users/Dell/OneDrive/Documents/3D-Assets/EnvironmentMap/daytime.hdr");
    }

    void update() override {
        ImGuiService::NewFrame();
        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("Options")) {
                ImGui::MenuItem("Load HDRI");
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        hdri_ui->show_options();
    }

    ~MainApplication() {
        ImGuiService::Shutdown();
    }

  private:
    std::shared_ptr<HDRIConverterPass> hdri_pass;
    std::unique_ptr<HDRIConverterUI> hdri_ui;
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