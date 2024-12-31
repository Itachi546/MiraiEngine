#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class SkyboxMaterial;

    class Overlay3DPass : public FrameGraphRenderer {
      public:
        Overlay3DPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

      private:
        std::shared_ptr<SkyboxMaterial> skybox_material;
        UniformSetID skybox_uniform_set;

        void render_skybox(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass);
        void render_debug_draw(CommandBuffer *command_buffer, Scene* scene, FrameGraphRenderpassInfo *render_pass);
    };

} // namespace mirai