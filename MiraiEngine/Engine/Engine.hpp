#pragma once

#include <memory>
#include "App.hpp"

namespace mirai
{

    class Window;

    class Engine
    {
      public:
        Engine();

        static Engine *get()
        {
            return Instance;
        }

        void set_app(std::unique_ptr<App> &&app) { this->app = std::move(app); }

        void run();

        void request_close()
        {
            this->running = false;
        }

        float get_dt_seconds() const
        {
            return static_cast<float>(dt_ms) / 1000.0f;
        }

        float get_elapsed_seconds() const
        {
            return static_cast<float>(elapsed_time_ms) / 1000.0f;
        }

        Engine(const Engine &) = delete;
        Engine operator=(const Engine &) = delete;

        ~Engine();

      private:
        uint64_t dt_ms;
        uint64_t elapsed_time_ms;
        static Engine *Instance;
        std::unique_ptr<App> app;
        std::unique_ptr<Window> window;
        bool running;
    };
} // namespace mirai