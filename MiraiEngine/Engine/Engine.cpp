#include "Engine.hpp"
#include "Device/Window.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/MaterialCache.hpp"
#include "Log.hpp"

#include <chrono>
#include <filesystem>
using namespace std::chrono_literals;

namespace mirai
{
    Engine *Engine::Instance = nullptr;

    Engine::Engine() : running(true), dt_ms(16), elapsed_time_ms(0)
    {
        Log::Info("Working Directory: ", std::filesystem::current_path());
        Log::Info("Initializing Engine ...");
        window = std::make_unique<Window>(1360, 769, "MiraiEngine");
        renderer = std::make_unique<Renderer>();
        material_cache = std::make_unique<MaterialCache>();
        Instance = this;
    }

    void Engine::run()
    {
        if (app)
            app->start();

        auto start = std::chrono::high_resolution_clock::now();
        while (running && !window->is_closed())
        {
            window->update();

            if (app)
                app->update();

            renderer->update();

            renderer->render();

            std::this_thread::sleep_for(16ms);

            auto end = std::chrono::high_resolution_clock::now();
            dt_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

            start = end;
            elapsed_time_ms += dt_ms;
        }
    }

    Engine::~Engine()
    {
        app = nullptr;
        window = nullptr;
    }

} // namespace mirai