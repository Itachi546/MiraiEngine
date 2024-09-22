#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Material.hpp"
#include "ScenePass/ForwardPass.hpp"

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
        Renderer::get()->register_scene_pass(std::make_unique<ForwardPass>(width, height));

        Scene *scene = Renderer::get()->get_scene();

        Material *mat = new FullScreenMaterial();

        mat->set_cull_mode(CULL_MODE_FRONT);
        /*
        Log::Debug("Hash: ", mat->get_hash());

        Log::Debug("Hash: ", mat->get_hash());

        mat->set_cull_mode(CULL_MODE_NONE);
        Log::Debug("Hash: ", mat->get_hash());

        mat->set_depth_test(true);
        Log::Debug("Hash: ", mat->get_hash());
        mat->set_depth_test(false);
        Log::Debug("Hash: ", mat->get_hash());
        */
        Material *mat2 = new FullScreenMaterial();
        Log::Debug("HashMat2: ", mat2->get_hash() == mat->get_hash());

        delete mat;
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