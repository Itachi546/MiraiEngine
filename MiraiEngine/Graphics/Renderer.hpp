#pragma once

#include <memory>

namespace mirai
{
    class VulkanRenderingDevice;

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

        ~Renderer();

      private:
        static Renderer *Instance;

        std::unique_ptr<VulkanRenderingDevice> device;
    };
} // namespace mirai