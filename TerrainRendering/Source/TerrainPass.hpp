#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"
#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"

namespace mirai {

    class Camera;
    struct Shader;

    class TerrainPass : public FrameGraphRenderer {

      public:
        TerrainPass(uint32_t width, uint32_t height, uint32_t maxHeight, uint32_t cbt_depth);

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~TerrainPass();

      private:
        RenderingDevice *device;
        uint32_t width, height, maxHeight, cbt_depth;
        BufferID cbt_buffer, cbt_dispatch_indirect_buffer, cbt_draw_indirect_buffer;
        Shader* cbt_init_program, *cbt_sum_reduction_program, *cbt_sum_reduction_prepass_program, *cbt_subdivision_program;
        Shader* terrain_shader;
        Shader* terrain_shader_wireframe;

        UniformSetID cbt_init_set, cbt_vert_set, cbt_subdivision_set, cbt_sum_reduction_set, cbt_sum_reduction_prepass_set;
        float lod_factor = 0.0f;

        TextureID texture_heightmap;

        bool enable_wireframe = false;
        bool freeze_frustum = false;
        bool enable_sumreduction_prepass = true;

        struct PushConstantData {
            glm::mat4 VP;
            glm::vec4 frustum_planes[6];
            glm::vec4 subdivision_info;
            glm::vec4 dims;
        } push_constant_data;
        // Merge - 0, Split - 1
        float subdivision_mode = 0.0f;

        void init_at_depth(uint32_t depth);

        void compute_sum_reduction_prepass(CommandBuffer *command_buffer);

        void compute_sum_reduction(CommandBuffer *command_buffer);

        void update_subdivision(CommandBuffer *command_buffer, Camera *camera);
    };
} // namespace mirai