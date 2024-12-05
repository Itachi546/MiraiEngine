#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ProceduralSkyMaterial;

    class Overlay3DPass : public FrameGraphRenderer {
      public:
        Overlay3DPass();

        void initialize(FrameGraph* frame_graph, const FrameGraphNode* node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

      private:
        std::shared_ptr<ProceduralSkyMaterial> material;
    };

} // namespace mirai