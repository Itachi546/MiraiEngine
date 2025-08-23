#include "Engine/App.hpp"
#include "Engine/Engine.hpp"

#include <memory>

using namespace mirai;

class TerrainApplication : public App {
  public:
    TerrainApplication() : App("TerrainApplication") {
    }

    void start() override {
    }

    void update() override {
    }

    ~TerrainApplication() {
    }
};

int main(int argc, char **argv) {
    EngineInitializationOptions options = {
        .width = 1360,
        .height = 769,
    };

    std::unique_ptr<Engine>
        engine = std::make_unique<Engine>(options);
    engine->set_app(std::make_unique<TerrainApplication>());
    engine->run();
    engine = nullptr;
}