#include "Mirai.hpp"

using namespace mirai;

class MainApplication : public App {
  public:
    MainApplication() : App("MiraiTools") {
    }

    void start() override {
        Window::get()->set_title("Mirai Tools");
    }

    void update() override {}

  private:
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