#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"

using namespace mirai;

class TestApplication : public App
{
  public:
    TestApplication() : App("TestApplication")
    {
        Window::get()->set_title("TestApplication");
    }

    void start() override
    {
    }

    void update() override
    {
        float time = Engine::get()->get_elapsed_seconds();
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