#pragma once

#include "Scene/FrameGraph.hpp"
namespace mirai
{
    class ShaderMaterial;
    class ForwardPass : public FrameGraphRenderPass
    {
      public:
        ForwardPass(const std::string &name);

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        ~ForwardPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        BufferID draw_indirect_buffer;

        struct DrawIndirectCommand
        {
            uint32_t index_count;
            uint32_t instance_count;
            uint32_t first_index;
            uint32_t vertex_offset;
            uint32_t first_instance;
        };

        UniformSetID mesh_instance_set;
        DrawIndirectCommand *draw_indirect_array;
    };
} // namespace mirai