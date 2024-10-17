#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai
{
    class ForwardPass : public FrameGraphRenderPass
    {
      public:
        ForwardPass(const std::string &name);

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        ~ForwardPass();
    };
} // namespace mirai