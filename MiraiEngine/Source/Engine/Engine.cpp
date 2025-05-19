#include "Engine.hpp"
#include "Device/Window.hpp"
#include "Graphics/Renderer.hpp"
#include "Log.hpp"
#include "Timer.hpp"
#include "Device/InputDevice.hpp"
#include "Profiler.hpp"

#include <chrono>
#include <filesystem>
using namespace std::chrono_literals;

namespace mirai {
    Engine *Engine::Instance = nullptr;

    Engine::Engine(const EngineInitializationOptions &options) : running(true), dt_ms(16), elapsed_time_ms(0) {
        ASSERT(Instance == nullptr);

        Instance = this;
        Timer timer;
        Log::Info("Working Directory: ", std::filesystem::current_path());
        Log::Info("Initializing Engine ...");
        window = std::make_unique<Window>(options.width, options.height, "MiraiEngine");

        renderer = std::make_unique<Renderer>();
        Log::Info("Initialization: ", timer.elapsed_seconds(), "s");
    }

    void Engine::run() {
        {
            Timer timer;
            if (app)
                app->start();
            Log::Info("Initialization: ", timer.elapsed_seconds(), "s");
            renderer->on_initialize();
        }

        auto start = std::chrono::steady_clock::now();
        while (running && !window->is_closed()) {
            {
                ScopedCpuProfiling("CPU Time");
                window->update();
                if (window->is_minimized())
                    continue;

                if (app)
                    app->update();

                renderer->update();

                renderer->render();

                Input::get()->update();
            }

            auto end = std::chrono::steady_clock::now();
            dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            elapsed_time_ms += dt_ms;
            start = end;
        }
    }

    Engine::~Engine() {
        RenderingDevice::get()->wait();
        app = nullptr;
        renderer = nullptr;
        window = nullptr;
    }

} // namespace mirai