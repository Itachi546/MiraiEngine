#include "Engine/App.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Graphics/Renderer.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Utils/FirstPersonController.hpp"
#include "Scene/GLTFLoader.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Math/MathUtils.hpp"

#include "ImGuiService.hpp"
#include "ImGuiRenderPass.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App {
  public:
    TestApplication(const std::vector<std::string> &model_paths) : App("TestApplication"), model_paths(model_paths) {
        Window::get()->set_title("TestApplication");
        Window::get()->set_fullscreen(fullscreen);
    }

    void start() override {
        ImGuiService::Initialize();

        uint32_t width = 1920;
        uint32_t height = 1080;

        scene = Renderer::get()->get_scene();

        std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/warm_bar_2k.hdr");
        scene->set_environment_map(env_map);

        Camera *camera = scene->get_camera();
        // camera->position = glm::vec3(3.0f, 2.0f, 0.0f);
        // camera->rotation = glm::vec3(0.0f, -90.0f, 0.0f);
        camera->set_far_plane(200.0f);
        // Create RenderPass
        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
#if 0
        frame_graph->load_from_file("Assets/forward_pass.json");
        frame_graph->set_renderer("forward_pass", std::make_shared<ForwardPass>());
        frame_graph->set_renderer("depth_prepass", std::make_shared<DepthPrePass>());
#else
        frame_graph->load_from_file("Assets/deferred_pass.json");
        frame_graph->set_renderer("deferred_pass", std::make_shared<DeferredPass>());
        frame_graph->set_renderer("deferred_lighting_pass", std::make_shared<DeferredLightingPass>());
#endif
        frame_graph->set_renderer("ssao_pass", std::make_shared<SSAOPass>());
        frame_graph->set_renderer("cascaded_shadow_pass", std::make_shared<CascadedShadowPass>());
        frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
        frame_graph->set_renderer("debug_pass", std::make_shared<DebugPass>());
        frame_graph->set_renderer("overlay3D", std::make_shared<Overlay3DPass>());
        frame_graph->set_renderer("imgui_pass", std::make_shared<ImGuiRenderPass>());
        frame_graph->compile();

        if (model_paths.size() > 0) {
            for (const auto &path : model_paths)
                ImportModel_GLTF(path, scene);
        }

        controller = std::make_unique<FirstPersonController>(scene->get_camera());
        controller->set_walk_speed(20.0f);
        controller->set_run_speed(10.0f);
    }

    void update() override {
        ImGuiService::NewFrame();
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();

        float dt = Engine::get()->get_dt_seconds();
        controller->update(Engine::get()->get_dt_seconds());

        if (Input::get()->was_down(KB_F)) {
            fullscreen = !fullscreen;
            Window::get()->set_fullscreen(fullscreen);
        }

        if (Input::get()->was_down(KB_1)) {
            show_debug_ui = !show_debug_ui;
        }

        add_debug_ui();
    }

    void add_profiler_ui() {
        if (!miProfiler::IsEnabled())
            return;

        std::vector<std::pair<std::string, float>> profiler_data;
        miProfiler::GetProfilerOutput(profiler_data);
        if (profiler_data.size() == 0)
            return;

        if (ImGui::CollapsingHeader("Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
            for (auto &[name, time] : profiler_data) {
                ImGui::Text("%s: %.2f", name.c_str(), time);
            }
        }
    }

    void add_debug_ui() {
        if (show_debug_ui) {
            ImGui::Begin("Debug UI", 0);
            uint64_t memory_usage = RenderingDevice::get()->get_memory_usage();
            ImGui::Text("GPU Memory Usage: %.2f MB", utils::bytes_to_mb(memory_usage));
            add_profiler_ui();
            ImGui::End();
        }
    }

    ~TestApplication() {
        ImGuiService::Shutdown();
        Log::Info("Destroying Test Application...");
    }

  private:
    bool show_debug_ui = false;
    bool fullscreen = false;
    Scene *scene;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
    };

    std::vector<std::string> model_paths;
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            model_paths.push_back(argv[i]);
        }
    }
    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    Log::Info("Creating Test Application ...");
    engine->set_app(std::make_unique<TestApplication>(model_paths));
    engine->run();
    engine = nullptr;
}