#pragma once

#include <stdint.h>
#include "Graphics/RenderingDevice.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"

namespace mirai {

    class Camera;

    class TerrainPass : public FrameGraphRenderer {

      public:
        TerrainPass(uint32_t width, uint32_t height, uint32_t cbt_depth);

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~TerrainPass();

      private:
        RenderingDevice *device;
        uint32_t width, height, cbt_depth;
        BufferID cbt_buffer, cbt_leaf_count_buffer, cbt_draw_indirect_buffer;
        std::unique_ptr<ComputeShader> cbt_init_program, cbt_sum_reduction_program, cbt_sum_reduction_prepass_program, cbt_subdivision_program;
        std::unique_ptr<ShaderMaterial> terrain_shader;
        std::unique_ptr<ShaderMaterial> terrain_shader_wireframe;

        UniformSetID cbt_init_set, cbt_vert_set, cbt_subdivision_set, cbt_sum_reduction_set;
        uint32_t *cbt_leaf_count_ptr = nullptr;
        float lod_factor = 0.0f;

        TextureID texture_heightmap;

        bool enable_wireframe = false;

        struct PushConstantData {
            glm::mat4 VP;
            glm::vec4 frustum_planes[6];
            glm::vec4 subdivision_info;
        } push_constant_data;
        bool freeze_frustum = false;
        // Merge - 0, Split - 1
        float subdivision_mode = 0.0f;

        void init_at_depth(uint32_t depth);

        void compute_sum_reduction_prepass(CommandBuffer *command_buffer);

        void compute_sum_reduction(CommandBuffer *command_buffer);

        void update_subdivision(CommandBuffer *command_buffer, Camera *camera);
    };
} // namespace mirai