#pragma once

#include "Scene/FrameGraph.hpp"
#include <memory>

namespace mirai
{
    class ShaderMaterial;
    struct FrameGraphNode;
    class FrameGraph;

    class FullScreenPass : public FrameGraphRenderPass
    {
      public:
        FullScreenPass(const std::string &name);

        void initialize(FrameGraph* frame_graph, const FrameGraphNode* node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode* node, Scene *scene) override;

        void set_antialiasing(bool state)
        {
            this->enable_aa = state;
        }

        ~FullScreenPass();

      private:
        std::shared_ptr<ShaderMaterial> material;
        bool enable_aa;
    };
} // namespace mirai