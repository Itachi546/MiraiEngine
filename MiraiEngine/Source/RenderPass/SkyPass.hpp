#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/Sky.hpp"

namespace mirai {

    class SkyPass : public FrameGraphRenderPass {
      public:
        SkyPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

      private:
        std::shared_ptr<ProceduralSkyMaterial> material;
    };

} // namespace mirai