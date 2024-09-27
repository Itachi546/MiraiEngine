#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/Material.hpp"
#include "RenderPass/ForwardPass.hpp"
#include "RenderPass/FullScreenPass.hpp"
#include "Scene/Component.hpp"
#include "Scene/FrameGraph.hpp"

#include "FullScreenMaterial.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App
{
  public:
    TestApplication() : App("TestApplication")
    {
        Window::get()->set_title("TestApplication");
    }

    void start() override
    {
        uint32_t width, height;
        Window::get()->get_size(&width, &height);
        Scene *scene = Renderer::get()->get_scene();

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

        // Create Entity
        FullScreenMaterial material;
        material.set_front_face(FRONT_FACE_CLOCKWISE);
        material.set_depth_write(true);
        material.set_depth_test(true);
        
        Entity entity = ecs::create_entity();
        scene->get_component_manager()->add_component<Material>(entity, material);
        scene->get_component_manager()->add_component<NameComponent>(entity, "test");
        scene->add_entity(entity);
    }

    void update() override
    {
        float time = Engine::get()->get_elapsed_seconds();
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();
    }

    ~TestApplication()
    {
        Log::Info("Destroying Test Application...");
    }
};

int main()
{
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
        .enable_validation = true,
    };

    std::unique_ptr<Engine> engine = std::make_unique<Engine>(options);
    engine->set_app(std::make_unique<TestApplication>());
    engine->run();
    engine = nullptr;
}