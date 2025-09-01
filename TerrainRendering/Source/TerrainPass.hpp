#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/FrameGraph.hpp"

namespace mirai {

    class TerrainPass : public FrameGraphRenderer {

      public:
        TerrainPass(uint32_t width, uint32_t height, uint32_t cbt_depth);

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~TerrainPass();

      private:
        RenderingDevice *device;
        uint32_t width, height, cbt_depth;
        BufferID cbt_buffer;
        std::unique_ptr<ComputeShader> cbt_init_program, cbt_sum_reduction_program;
        std::unique_ptr<ShaderMaterial> terrain_shader;
        UniformSetID cbt_buffer_comp_set, cbt_buffer_vert_set;

        void reset_at_depth(uint32_t depth);

        void compute_sum_reduction(CommandBuffer *command_buffer);
    };
} // namespace mirai