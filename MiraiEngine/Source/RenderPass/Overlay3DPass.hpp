#pragma once

#include "Scene/FrameGraph.hpp"

namespace mirai {
    class ShaderMaterial;
    struct LineRenderer;
    class Overlay3DPass : public FrameGraphRenderer {
      public:
        Overlay3DPass();

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;
        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

      private:
        ShaderMaterial *skybox_material;
        UniformSetID skybox_uniform_set;
        SamplerID default_sampler;

        std::unique_ptr<LineRenderer> line_renderer;

        void render_skybox(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass);
        void render_debug_draw(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass);
    };

} // namespace mirai