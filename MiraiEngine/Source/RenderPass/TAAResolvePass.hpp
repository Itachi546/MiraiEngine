#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"

namespace mirai {

    struct Shader;

    class TAAResolvePass : public FrameGraphRenderer {
      public:
        TAAResolvePass() : FrameGraphRenderer("taa_resolve_pass") {}

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        bool should_sample_motion_vector = true;

      private:
        bool first_frame = true;
        Shader *shader, *copy_texture_shader;
        UniformSetID uniform_set, copy_texture_uniform_set;
    };

} // namespace mirai