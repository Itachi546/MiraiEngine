#pragma once

#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"
#include "Scene/Scene.hpp"

namespace mirai {

    class ShaderMaterial;

    class CascadedShadowPass : public FrameGraphRenderer {
      public:
        CascadedShadowPass() : FrameGraphRenderer("directional_shadow_pass") {
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene* scene) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~CascadedShadowPass();

        float split_lamda = 0.9f;
        float shadow_distance = 80.0f;
        uint32_t shadow_map_size = 2048;
        ShaderMaterial* shader;

        std::array<float, NUM_DIRLIGHT_CASCADE> split_distances_constants = {5.0f, 15.0f, 40.0f, 100.0f};
        bool calculate_distance_automatic = false;

      private:
        void calculate_split_distances(float znear, float zfar, Scene *scene);
        UniformSetID mesh_instance_set;
    };
} // namespace mirai