#pragma once

#include "Math/Frustum.hpp"
#include "Shader.hpp"

namespace mirai {
    class Scene;

    struct BatchBuildParams {
        const FrustumPlanes *frustum = nullptr;
        const glm::vec3 *camera_position = nullptr;
        PassMode pass = PASS_MODE_FORWARD;
    };

    struct MeshDrawInfo {
        uint32_t transform_index;
        uint32_t material_index;
        DrawIndexedIndirectCommand draw_info;
        // Distance from camera, used for sorting
        uint32_t vertex_stride;
        float distance_to_camera_sqr;
    };

    struct RenderBatch {
        uint64_t sort_key;

        std::vector<MeshDrawInfo> draw_infos;

        // Populated and only used for draw_indexed_indirect
        BufferView draw_indirect_buffer_view;

        // For draw data, memory is allocated in per frame staging buffer and this field is populated
        DescriptorOffset draw_data_descriptor;

        BufferID get_geometry_buffer() const {
            return BufferID{sort_key & 0xFFFFFFFFu};
        }

        AlphaMode get_alpha_mode() const {
            return AlphaMode((sort_key >> 37) & 0x3);
        }

        uint32_t get_pso_key() const {
            return sort_key >> 32;
        }

        void sort() {
            if (get_alpha_mode() == ALPHA_MODE_BLEND) {
                std::sort(draw_infos.begin(), draw_infos.end(), [](const MeshDrawInfo &lhs, const MeshDrawInfo &rhs) {
                    return lhs.distance_to_camera_sqr > rhs.distance_to_camera_sqr;
                });
            } else {
                std::sort(draw_infos.begin(), draw_infos.end(), [](const MeshDrawInfo &lhs, const MeshDrawInfo &rhs) {
                    return lhs.distance_to_camera_sqr < rhs.distance_to_camera_sqr;
                });
            }
        }
    };

    struct DrawBatchGenerator {
        static void BuildBatches(Scene *scene, const BatchBuildParams &build_params, std::vector<RenderBatch> &out_batches);
    };

    struct BatchPushData {
        void *data;
        uint32_t offset;
        uint32_t size;
    };

    struct BatchDrawInfo {
        Shader *shader;
        std::vector<DescriptorOffset> descriptor_infos;
        BatchPushData *push_data;
    };

    void DrawBatch(CommandBuffer *command_buffer, const RenderBatch &render_batch, const BatchDrawInfo &batch_info);

} // namespace mirai