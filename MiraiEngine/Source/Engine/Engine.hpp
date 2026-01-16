#pragma once

#include <memory>
#include "App.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "AppSettings.hpp"

namespace mirai {

    class Window;
    class Renderer;

    struct EngineInitializationOptions {
        uint32_t width = AppSettings::default_window_width, height = AppSettings::default_window_height;
        RenderMode render_mode = AppSettings::render_mode;
    };

    class Engine {
      public:
        Engine(const EngineInitializationOptions &options);

        static Engine *get() {
            return Instance;
        }

        void set_app(std::unique_ptr<App> &&app) { this->app = std::move(app); }

        void run();

        void request_close() {
            this->running = false;
        }

        float get_dt_seconds() const {
            return static_cast<float>(dt_ms) / 1000.0f;
        }

        void set_show_metrics(bool state) {
            this->show_metrics = state;
        }

        float get_elapsed_seconds() const {
            return static_cast<float>(elapsed_time_ms) / 1000.0f;
        }

        Engine(const Engine &) = delete;
        Engine operator=(const Engine &) = delete;

        ~Engine();

        bool show_metrics;

      private:
        uint64_t dt_ms;
        uint64_t elapsed_time_ms;
        static Engine *Instance;

        std::unique_ptr<App> app;
        std::unique_ptr<Window> window;
        std::unique_ptr<Renderer> renderer;

        bool running;
    };
} // namespace mirai