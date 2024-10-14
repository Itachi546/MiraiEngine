#pragma once

#include "Scene/FrameGraph.hpp"
#include <memory>

namespace mirai
{
    class Material;
    class FullScreenPass : public FrameGraphRenderPass
    {
      public:
        FullScreenPass(const std::string &name);

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, Scene *scene) override;

        void set_antialiasing(bool state)
        {
            this->enable_aa = state;
        }

        ~FullScreenPass();

      private:
        std::shared_ptr<Material> material;
        bool enable_aa;
    };
} // namespace mirai