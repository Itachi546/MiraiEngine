#pragma once

#include "ScenePass.hpp"
namespace mirai
{
    class ForwardPass : public ScenePass
    {
      public:
        ForwardPass(uint32_t width, uint32_t height);

        void update();

        void render(CommandBuffer *command_buffer, Scene *scene);

        ~ForwardPass();
    };
} // namespace mirai