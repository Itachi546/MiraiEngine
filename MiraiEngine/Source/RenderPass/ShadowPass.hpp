#pragma once

#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"

namespace mirai {

    class ShaderMaterial;

    class CascadedShadowPass : public FrameGraphRenderer {
      public:
        CascadedShadowPass() : FrameGraphRenderer("cascaded_shadow_pass") {
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

      private:
        std::shared_ptr<ShaderMaterial> shader;
        uint32_t cascade_count = 5;
        float split_lamda = 0.9f;
        float shadow_distance = 80.0f;

        uint32_t shadow_map_size = 2048;

        std::vector<float> split_distances;
        std::vector<glm::mat4> cascade_VP;
        void calculate_split_distance(float znear, float zfar);
    };
} // namespace mirai