// #include "Engine/App.hpp"
// #include "Engine/Engine.hpp"
// #include "TerrainPass.hpp"
// #include "Device/Window.hpp"
// #include "Scene/Scene.hpp"
// #include "Scene/EnvironmentMap.hpp"
// #include "Graphics/TextRenderManager.hpp"
// #include "Graphics/Renderer.hpp"
// #include "RenderPass/RenderPass.hpp"
// #include "Utils/FirstPersonController.hpp"
// #include <memory>

// using namespace mirai;

// class TerrainApplication : public App {
//   public:
//     TerrainApplication() : App("TerrainApplication") {
//         Window::get()->set_title("TerrainApplication");
//     }

//     void start() override {
//         Scene *scene = Renderer::get()->get_scene();
//         std::shared_ptr<EnvironmentMap> env_map = std::make_shared<EnvironmentMap>("Assets/Envmap/daytime.hdr");
//         scene->set_environment_map(env_map);

//         Renderer::get()->set_pipeline_description_file("Assets/pipelines.json");
//         FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
//         frame_graph->load_from_file("Assets/terrain_pass.json");
//         frame_graph->set_renderer("terrain_pass", std::make_shared<TerrainPass>(32000, 32000, 2400, 27));
//         frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
//         frame_graph->set_renderer("sky_pass", std::make_shared<Overlay3DPass>());
//         frame_graph->set_renderer("debug_pass", std::make_shared<DebugPass>());

//         Camera *camera = scene->get_camera();
//         camera->set_near_plane(3.0f);
//         camera->set_far_plane(64000.0f);
//         camera->position = glm::vec3(1000.0f, 500.0f, 1000.0f);
//         controller = std::make_unique<FirstPersonController>(camera);
//         controller->set_walk_speed(1000.0f);
//         controller->set_run_speed(10000.0f);
//     }

//     void update() override {
//         if (Input::get()->is_down(KB_ESCAPE))
//             Engine::get()->request_close();

//         float dt = Engine::get()->get_dt_seconds();
//         controller->update(dt);

//         std::vector<miProfiler::ProfilerOutput> cpu_outputs, gpu_outputs;
//         miProfiler::GetProfilerOutput(cpu_outputs, gpu_outputs);
//         TextRenderer *renderer = TextRenderManager::get()->get_default();

//         glm::vec2 position{20.0f, 20.0f};
//         for (miProfiler::ProfilerOutput output : gpu_outputs) {
//             renderer->AddText(output.name + ": " + std::to_string(output.time_in_ms), position);
//             position.y += 20.0f;
//         }
//         for (miProfiler::ProfilerOutput output : cpu_outputs) {
//             renderer->AddText(output.name + ": " + std::to_string(output.time_in_ms), position);
//             position.y += 20.0f;
//         }
//     }

//     ~TerrainApplication() {
//     }

//   private:
//     std::unique_ptr<FirstPersonController> controller;
// };

int main(int argc, char **argv) {
    /*
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
    };

    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    engine->set_app(std::make_unique<TerrainApplication>());
    engine->run();
    engine = nullptr;
    */
}