#pragma once

#include "Scene/FrameGraph.hpp"
#include "Math/Math.hpp"
#include "Scene/Scene.hpp"

namespace mirai {

    class ShaderMaterial;

    class CascadedShadowPass : public FrameGraphRenderer {
      public:
        CascadedShadowPass() : FrameGraphRenderer("cascaded_shadow_pass") {
        }

        void initialize(FrameGraph *frame_graph, const FrameGraphNode *node) override;

        void update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) override;

        void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override;

        ~CascadedShadowPass();

      private:
        std::shared_ptr<ShaderMaterial> shader;
        float split_lamda = 0.85f;
        float shadow_distance = 150.0f;

        uint32_t shadow_map_size = 2048;

        std::vector<DrawData> opaque_batches;
        std::vector<DrawData> transparent_batches;

        void calculate_split_distances(float znear, float zfar, Scene *scene);

        UniformSetID mesh_instance_set;
    };
} // namespace mirai