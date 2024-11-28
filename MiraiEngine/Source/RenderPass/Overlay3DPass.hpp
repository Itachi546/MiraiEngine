#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ProceduralSkyMaterial;

    class Overlay3DPass : public FrameGraphRenderPass {
      public:
        Overlay3DPass();

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

      private:
        std::shared_ptr<ProceduralSkyMaterial> material;
    };

} // namespace mirai