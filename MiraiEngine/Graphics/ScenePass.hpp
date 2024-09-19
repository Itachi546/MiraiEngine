#pragma once

#include "RenderingDevice.hpp"

namespace mirai
{
    class CommandBuffer;

    class ScenePass
    {
      public:
        ScenePass(const RenderPass &render_pass) : render_pass(render_pass)
        {
        }

        virtual void update() = 0;

        virtual void render(CommandBuffer *cb) = 0;

        virtual ~ScenePass() = default;

        RenderPass *get_render_pass() { return &render_pass; }

      protected:
        RenderPass render_pass;
    };
}; // namespace mirai