#pragma once
#include "Scene/FrameGraph.hpp"
#include "Scene/Scene.hpp"
namespace mirai {

    struct Shader;
    class CascadedShadowPass : public FrameGraphRenderer {
      public:
        CascadedShadowPass() : FrameGraphRenderer("directional_shadow_pass") {
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) override;

        ~CascadedShadowPass();

        float split_lamda = 0.909f;
        float shadow_distance = 100.0f;
        uint32_t shadow_map_size = 2048;

        std::array<float, NUM_DIRLIGHT_CASCADE> split_distances_constants = {5.0f, 15.0f, 40.0f, 100.0f};
        bool calculate_distance_automatic = true;

      private:
        UniformSetID create_draw_data_binding(CommandBuffer *command_buffer, BufferID buffer, uint32_t offset, uint32_t size);
        void calculate_split_distances(float znear, float zfar, Scene *scene);
        Shader *shader;
    };
} // namespace mirai