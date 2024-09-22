#pragma once

#include <memory>
#include <vector>

namespace mirai
{
    class RenderingDevice;
    class CommandBuffer;

    class Renderer
    {
      public:
        Renderer();
        Renderer(const Renderer &) = delete;
        Renderer operator=(const Renderer &) = delete;

        static Renderer *get()
        {
            return Instance;
        }

        void update();

        void render();

        ~Renderer();

      private:
        static Renderer *Instance;
        std::unique_ptr<RenderingDevice> device;
    };
} // namespace mirai