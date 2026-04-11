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
        RENDERBATCH_TYPE_TRANSPARENT = 4,
        RENDERBATCH_TYPE_SKINNED = 8
    };

    enum BatchFilterFlag {
        BATCH_FILTER_FLAG_OPAQUE = 1,
        BATCH_FILTER_FLAG_ALPHA_MASK = 2,
        BATCH_FILTER_FLAG_TRANSPARENT = 4,
        BATCH_FILTER_SKIP_NEAR_PLANE = 8,
        BATCH_FILTER_FLAG_SKINNED = 16
    };

    struct MeshDrawInfo {
        DrawIndexedIndirectCommand draw_info;
        uint32_t material_index;
        uint32_t transform_index;
        // Distance from camera, used for sorting
        float distance_to_camera_sqr;
        uint32_t vertex_stride;

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
    /*
    struct ShadowMeshBatch {
        BufferView vertex_buffer;
        BufferView index_buffer;

        std::vector<MeshDrawInfo> mesh_draw_infos;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t vertex_stride) {
            mesh_draw_infos.emplace_back(transform_index, material_index, vertex_offset, index_offset, index_count, 0.0f, vertex_stride);
        }
        RenderBatchType render_batch_type;
    };
    */
    struct RenderBatch {
        uint32_t sort_key;
        RenderBatchType batch_type;
        std::vector<MeshBatch> meshes;

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

    struct DrawBatchGenerator {
        static void CreateBatch(const Scene *scene, const FrustumPlanes *frustum, const glm::vec3 &camera_position, std::vector<RenderBatch> &render_batches, uint32_t batch_filter_flags);

        // Used for Shadow/Cascaded shadow rendering where scene needs to be culled again
        static void CreateShadowMeshBatch(const Scene *scene, const FrustumPlanes *frustum, std::vector<RenderBatch> &render_batches, uint32_t batch_filter_flags);
    };

    struct BatchDrawInfo {
        Shader *shader;
        std::vector<DescriptorOffset> descriptor_infos;
        PushData *push_data;
        uint32_t draw_data_descriptor_index;
    };

    void DrawBatch(CommandBuffer *command_buffer, const RenderBatch &render_batch, const BatchDrawInfo &batch_info);
} // namespace mirai
