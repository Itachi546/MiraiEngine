// #pragma once

// #include "Scene/FrameGraph.hpp"
// #include "Math/Math.hpp"

// #include <memory>

// namespace mirai {

//     struct Shader;

//     class DirectionalShadowPassRT : public FrameGraphRenderer {
//       public:
//         DirectionalShadowPassRT() : FrameGraphRenderer("rt_directional_shadow_pass") {
//         }

//         void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *scene) override;

//         void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *scene) override;

//         ~DirectionalShadowPassRT();

//         Shader *dir_shadow_shader;
//         Shader *blur_shader;

//         float sigma = 1.0f;
//         float blur_sample_count = 10.0f;

//       private:
//         UniformSetID rt_uniform_set, blur_uniform_set_x, blur_uniform_set_y;
//         TextureID blur_intermediate_texture;

//         void render_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
//         void blur_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene);
//     };
// } // namespace mirai