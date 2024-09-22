#pragma once

#include <memory>
#include <vector>

#include "ScenePass/ScenePass.hpp"

namespace mirai
{
    class RenderingDevice;
    class CommandBuffer;
    class Scene;

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

        void register_scene_pass(std::unique_ptr<ScenePass> scene_pass)
        {
            scene_passes.push_back(std::move(scene_pass));
        }

        void set_scene(std::unique_ptr<Scene> scene)
        {
            this->scene = std::move(scene);
        }

        Scene *get_scene()
        {
            return scene.get();
        }

        void compile_passes();

        void update();

        void render();

        ~Renderer();

      private:
        static Renderer *Instance;

        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::vector<std::unique_ptr<ScenePass>> scene_passes;
    };
} // namespace mirai