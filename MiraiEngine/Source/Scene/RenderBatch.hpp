#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Material.hpp"
#include <vector>
#include <Math/Math.hpp>
#include <algorithm>

namespace mirai {
    class Scene;
    class CommandBuffer;
    struct FrustumPlanes;

    struct PushData {
        void *data;
        uint32_t offset;
        uint32_t size;
    };

    enum RenderBatchType {
        RENDERBATCH_TYPE_OPAQUE = 1,
        RENDERBATCH_TYPE_ALPHA_MASK = 2,
        // RENDERBATCH_TYPE_SKINNED = 4,
        RENDERBATCH_TYPE_TRANSPARENT = 8,
    };

    enum BatchFilterFlag {
        BATCH_FILTER_FLAG_OPAQUE = 1,
        BATCH_FILTER_FLAG_ALPHA_MASK = 2,
        BATCH_FILTER_FLAG_TRANSPARENT = 4,
        BATCH_FILTER_FLAG_SKINNED = 8,
    };

    struct MeshDrawInfo {
        uint32_t transform_index;
        uint32_t material_index;
        DrawIndexedIndirectCommand draw_info;
        // Distance from camera, used for sorting
        uint32_t vertex_stride;
        float distance_to_camera_sqr;

        MeshDrawInfo(uint32_t transform_index,
                     uint32_t material_index,
                     uint32_t vertex_offset_bytes,
                     uint32_t index_offset,
                     uint32_t index_count,
                     float distance_to_camera_sqr,
                     uint32_t vertex_stride) : transform_index(transform_index),
                                               material_index(material_index),
                                               draw_info{
                                                   index_count,
                                                   1,
                                                   index_offset,
                                                   vertex_offset_bytes,
                                                   0},
                                               vertex_stride(vertex_stride), distance_to_camera_sqr(distance_to_camera_sqr) {
        }
    };

    struct MeshBatch {
        BufferID vertex_buffer;
        BufferID index_buffer;

        // These are currently populated by renderer all at once for all batches
        // We can also move it inside this call or just store it once and forget about it
        BufferView draw_indirect_buffer_view;
        BufferView draw_data_buffer_view;

        std::vector<MeshDrawInfo> mesh_draw_infos;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, float distance_to_camera_sqr, uint32_t vertex_stride) {
            mesh_draw_infos.emplace_back(transform_index, material_index, vertex_offset, index_offset, index_count, distance_to_camera_sqr, vertex_stride);
        }
    };

    struct RenderBatch {
        uint32_t sort_key;
        RenderBatchType batch_type;
        std::vector<MeshBatch> meshes;

        // Non-null only when is_custom_sort_key(sort_key) == true (ShaderMaterial3D batch).
        // The draw layer uses this directly instead of a registry lookup.
        Shader *custom_shader = nullptr;

        void sort() {
            if (batch_type == RENDERBATCH_TYPE_TRANSPARENT) {
                for (auto &mesh_batch : meshes) {
                    std::sort(mesh_batch.mesh_draw_infos.begin(), mesh_batch.mesh_draw_infos.end(), [](const MeshDrawInfo &left, const MeshDrawInfo &right) {
                        return left.distance_to_camera_sqr > right.distance_to_camera_sqr;
                    });
                }
            } else {
                for (auto &mesh_batch : meshes) {
                    std::sort(mesh_batch.mesh_draw_infos.begin(), mesh_batch.mesh_draw_infos.end(), [](const MeshDrawInfo &left, const MeshDrawInfo &right) {
                        return left.distance_to_camera_sqr < right.distance_to_camera_sqr;
                    });
                }
            }
        }
    };

    // Bit 31 distinguishes a ShaderMaterial3D batch from a standard registry batch.
    // pipeline_id.id values are small sequential device integers; they never reach bit 31.
    constexpr uint32_t K_CUSTOM_SHADER_BIT = (1u << 31);

    inline uint32_t make_custom_sort_key(uint32_t pipeline_id) {
        return K_CUSTOM_SHADER_BIT | pipeline_id;
    }
    inline bool is_custom_sort_key(uint32_t sort_key) {
        return (sort_key & K_CUSTOM_SHADER_BIT) != 0;
    }

    struct BatchBuildParams {
        uint32_t filter_flags = BATCH_FILTER_FLAG_OPAQUE;
        const FrustumPlanes *frustum = nullptr;
        const glm::vec3 *camera_position = nullptr;
        const MaterialState *pass_state_override = nullptr;
        bool shadow_pass = false;
    };

    struct DrawBatchGenerator {
        static void BuildBatches(const Scene *scene, const BatchBuildParams &params, std::vector<RenderBatch> &out_batches);
    };

    struct BatchDrawInfo {
        Shader *shader;
        std::vector<DescriptorOffset> descriptor_infos;
        PushData *push_data;
        uint32_t draw_data_descriptor_index;
    };

    void DrawBatch(CommandBuffer *command_buffer, const RenderBatch &render_batch, const BatchDrawInfo &batch_info);
} // namespace mirai
