#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Common/Color.hpp"
#include "Graphics/ScenePass.hpp"
#include "Graphics/Renderer.hpp"

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class MainScenePass : public ScenePass
{
  public:
    MainScenePass(const RenderPass &render_pass) : ScenePass(render_pass)
    {
        Log::Info("Main Scene Created...");
    }

    void update() override
    {
    }

    void render(CommandBuffer *cb) override
    {
    }

    ~MainScenePass()
    {
        Log::Info("Main Scene Destroyed ...");
    }
};

class TestApplication : public App
{
  public:
    TestApplication() : App("TestApplication")
    {
        Window::get()->set_title("TestApplication");
    }

    void start() override
    {
        std::vector<Attachment> color_attachments = {
            Attachment{0, "main_color_attachment", ATTACHMENT_TYPE_SWAPCHAIN, FORMAT_UNDEFINED, Color_Black},
        };

        int width, height;
        Window::get()->get_size(&width, &height);

        RenderPass main_render_pass = {
            .color_attachments = color_attachments,
            .depth_attachments = {},
            .width = uint32_t(width),
            .height = uint32_t(height),
        };

        Renderer::get()->add_scene_pass(std::make_unique<MainScenePass>(main_render_pass));
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
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    engine->set_app(std::make_unique<TestApplication>());
    engine->run();
    engine = nullptr;
}