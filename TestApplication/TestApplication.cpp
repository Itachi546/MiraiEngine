#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "RenderPass/ForwardPass.hpp"
#include "RenderPass/FullScreenPass.hpp"
#include "Scene/Component.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/GLTFLoader.hpp"
#include "FullScreenMaterial.hpp"
#include "Utils/FirstPersonController.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App
{
  public:
    TestApplication(const std::vector<std::string> &model_paths) : App("TestApplication"), model_paths(model_paths)
    {
        Window::get()->set_title("TestApplication");
    }

    void start() override
    {
        uint32_t width = 1920;
        uint32_t height = 1080;
        scene = Renderer::get()->get_scene();

        // Create RenderPass
        FrameGraph *frame_graph = Renderer::get()->get_frame_graph();
        {
            // Forward Pass
            std::vector<FrameGraphResourceOutput> outputs = {
                FrameGraphResourceOutput{
                    COLOR_ATTACHMENT_OUTPUT_NAME,
                    FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    width,
                    height,
                    FORMAT_B8G8R8A8_UNORM,
                    LOAD_OP_CLEAR,
                    {0.0f, 0.0f, 0.0f, 1.0f},
                },
                FrameGraphResourceOutput{
                    DEPTH_ATTACHMENT_OUTPUT_NAME,
                    FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                    width,
                    height,
                    FORMAT_D32_SFLOAT_S8_UINT,
                    LOAD_OP_CLEAR,
                    {1.0f, 0.0f, 0.0f, 1.0f},
                },
            };

            FrameGraphNodeDescription node_description = {
                .name = "forward_pass",
                .enabled = true,
                .inputs = {},
                .outputs = outputs,
                .renderer = std::make_shared<ForwardPass>("forward_pass"),
            };
            frame_graph->add_node(node_description);
        }

        // Swapchain Copy
        {
            std::vector<FrameGraphResourceInput> inputs = {
                FrameGraphResourceInput{
                    COLOR_ATTACHMENT_OUTPUT_NAME,
                    FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
                },
            };

            std::vector<FrameGraphResourceOutput> outputs = {
                FrameGraphResourceOutput{
                    "swapchain",
                    FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN,
                    width,
                    height,
                    FORMAT_B8G8R8A8_UNORM,
                    LOAD_OP_CLEAR,
                    {0.0f, 0.0f, 0.0f, 1.0f},
                },
            };

            FrameGraphNodeDescription node_description = {
                .name = "swapchain_pass",
                .enabled = true,
                .inputs = inputs,
                .outputs = outputs,
                .renderer = std::make_shared<FullScreenPass>("swapchain_pass"),
            };
            frame_graph->add_node(node_description);
        }

        frame_graph->compile();

        if (model_paths.size() > 0)
        {
            for (const auto &path : model_paths)
                ImportModel_GLTF(path, scene);
        }

        controller = new CameraController(scene->get_camera());
    }

    void update() override
    {
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();

        controller->update(Engine::get()->get_dt_seconds() * 1000.0f);
    }

    ~TestApplication()
    {
        delete controller;
        Log::Info("Destroying Test Application...");
    }

  private:
    Scene *scene;
    const std::vector<std::string> &model_paths;
    CameraController *controller;
};

int main(int argc, char **argv)
{
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
        .enable_validation = true,
    };

    std::vector<std::string> model_paths;
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            model_paths.push_back(argv[i]);
        }
    }
    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    Log::Info("Creating Test Application ...");
    engine->set_app(std::make_unique<TestApplication>(model_paths));
    engine->run();
    engine = nullptr;
}