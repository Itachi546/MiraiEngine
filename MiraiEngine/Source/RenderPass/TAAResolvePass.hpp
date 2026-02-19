#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"

namespace mirai {

    struct Shader;

    class TAAResolvePass : public FrameGraphRenderer {
      public:
        TAAResolvePass() : FrameGraphRenderer("taa_resolve_pass") {
            taa_history_textures[0] = TextureID{K_INVALID_ID};
            taa_history_textures[1] = TextureID{K_INVALID_ID};
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        bool should_sample_motion_vector = true;
        bool enable_taa = true;
        bool enable_taa_simple = true;
        bool enable_temporal_filtering = true;
        bool should_reset_history_texture = true;
        bool should_enable_min_depth = true;
        bool should_enable_history_sampling = true;

        ~TAAResolvePass();

      private:
        Shader *shader;
        UniformSetID uniform_set[2];
        TextureID taa_history_textures[2];
        uint32_t current_taa_texture = 0;

        uint32_t taa_texture_width = 1920;
        uint32_t taa_texture_height = 1080;

        void reset_history_texture(CommandBuffer *command_buffer, FrameGraph *frame_graph);
    };

} // namespace mirai