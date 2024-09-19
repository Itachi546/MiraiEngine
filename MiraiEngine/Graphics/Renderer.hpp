#pragma once

#include <memory>
#include <vector>
#include "ScenePass.hpp"

namespace mirai
{
    class VulkanRenderingDevice;
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

        void add_scene_pass(std::unique_ptr<ScenePass> scene_pass)
        {
            scene_passes.push_back(std::move(scene_pass));
        }

        void update();

        void render();

        ~Renderer();

      private:
        static Renderer *Instance;
        std::vector<std::unique_ptr<ScenePass>> scene_passes;
        std::unique_ptr<VulkanRenderingDevice> device;

        void render_scene_pass(CommandBuffer *cb, std::unique_ptr<ScenePass> &scene_pass);
    };
} // namespace mirai