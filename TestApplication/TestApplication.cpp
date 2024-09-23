#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/Material.hpp"
#include "ScenePass/ForwardPass.hpp"
#include "Scene/Component.hpp"

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
        Renderer::get()->compile_passes();

        Scene *scene = Renderer::get()->get_scene();

        FullScreenMaterial material;
        material.set_front_face(FRONT_FACE_CLOCKWISE);

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
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    engine->set_app(std::make_unique<TestApplication>());
    engine->run();
    engine = nullptr;
}