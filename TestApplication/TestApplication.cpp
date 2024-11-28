#include "Mirai.hpp"

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
        uint32_t width = 1920;
        uint32_t height = 1080;
        scene = Renderer::get()->get_scene();

        // Create RenderPass
        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
#if 1 
        frame_graph->load_from_file("Assets/forward_pass.json");
        frame_graph->set_renderer("forward_pass", std::make_shared<ForwardPass>());
#else
        frame_graph->load_from_file("Assets/deferred_pass.json");
        frame_graph->set_renderer("gbuffer_pass", std::make_shared<GBufferPass>());
        frame_graph->set_renderer("deferred_pass", std::make_shared<DeferredPass>());
#endif
        frame_graph->set_renderer("swapchain_copy", std::make_shared<SwapchainCopyPass>());
        frame_graph->set_renderer("debug_pass", std::make_shared<DebugPass>());
        frame_graph->set_renderer("sky_pass", std::make_shared<SkyPass>());
        frame_graph->compile();

        if (model_paths.size() > 0) {
            for (const auto &path : model_paths)
                ImportModel_GLTF(path, scene);
        }

        Camera *camera = scene->get_camera();
        camera->position = glm::vec3(-25.0f, 2.0f, 10.0f);
        camera->rotation = glm::vec3(2.0f, 101.0f, 0.0f);
        controller = std::make_unique<FirstPersonController>(scene->get_camera());
    }

    void update() override {
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();

        float dt = Engine::get()->get_dt_seconds();
        controller->update(Engine::get()->get_dt_seconds());

        if (Input::get()->is_down(KB_F)) {
            fullscreen = !fullscreen;
            Window::get()->set_fullscreen(fullscreen);
        }
    }

    ~TestApplication() {
        Log::Info("Destroying Test Application...");
    }

  private:
    bool fullscreen = false;
    Scene *scene;
    const std::vector<std::string> &model_paths;
    std::unique_ptr<FirstPersonController> controller;
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
        .enable_validation = true,
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