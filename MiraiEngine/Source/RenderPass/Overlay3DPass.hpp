namespace mirai {
    class FrameGraph;
    class FrameGraphBlackBoard;

    class Overlay3DPass {
      public:
        Overlay3DPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board);
        ~Overlay3DPass() = default;
    };
} // namespace mirai

// #pragma once
// #include "Scene/FrameGraph.hpp"
// #include "Graphics/LineRenderer.hpp"

// namespace mirai {
//     struct Shader;
//     class Overlay3DPass : public FrameGraphRenderer {
//       public:
//         Overlay3DPass();

//         void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;
//         void update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

//         void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

//       private:
//         Shader *skybox_shader;
//         UniformSetID skybox_uniform_set;
//         SamplerID default_sampler;

//         std::unique_ptr<LineRenderer> line_renderer;

//         void render_skybox(CommandBuffer *command_buffer, Scene *scene);
//         void render_debug_draw(CommandBuffer *command_buffer, Scene *scene);
//     };

// } // namespace mirai