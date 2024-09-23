#pragma once

#include <memory>

#include "Scene/Scene.hpp"

namespace mirai
{
    class CommandBuffer;
    struct RenderPass;
    
    class ScenePass
    {
      public:
        ScenePass() = default;

        virtual void update() = 0;

        virtual void render(CommandBuffer *command_buffer, Scene *scene) = 0;

        virtual ~ScenePass() = default;

      protected:
        std::unique_ptr<RenderPass> render_pass;
    };
}; // namespace mirai