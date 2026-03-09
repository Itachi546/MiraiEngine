#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Material.hpp"
#include <vector>
#include <Math/Math.hpp>
#include <algorithm>

namespace mirai {
    class Scene;
    class CommandBuffer;

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
        float distance_to_camera;
        uint32_t vertex_stride;

        MeshDrawInfo(uint32_t transform_index,
                     uint32_t material_index,
                     uint32_t vertex_offset_bytes,
                     uint32_t index_offset,
                     uint32_t index_count,
                     float distance_to_camera,
                     uint32_t vertex_stride) : transform_index(transform_index),
                                               material_index(material_index),
                                               draw_info{
                                                   index_count,
                                                   1,
                                                   index_offset,
                                                   vertex_offset_bytes,
                                                   0},
                                               vertex_stride(vertex_stride), distance_to_camera(distance_to_camera) {
        }
    };

    struct MeshBatch {
        BufferView vertex_buffer;
        BufferView index_buffer;

        // These are currently populated by renderer all at once for all batches
        // We can also move it inside this call
        BufferView draw_indirect_buffer_view;
        BufferView draw_data_buffer_view;

        UniformSetID vertex_binding_set;

        std::vector<MeshDrawInfo> mesh_draw_infos;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, float distance_to_camera, uint32_t vertex_stride) {
            mesh_draw_infos.emplace_back(transform_index, material_index, vertex_offset, index_offset, index_count, distance_to_camera, vertex_stride);
        }
    };

    struct ShadowMeshBatch {
        BufferView vertex_buffer;
        BufferView index_buffer;

        UniformSetID vertex_binding_set;
        std::vector<MeshDrawInfo> mesh_draw_infos;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, uint32_t vertex_stride) {
            mesh_draw_infos.emplace_back(transform_index, material_index, vertex_offset, index_offset, index_count, 0.0f, vertex_stride);
        }
        RenderBatchType render_batch_type;
    };

    struct RenderBatch {
        ShaderPassKey shader_key;
        RenderBatchType batch_type;
        std::vector<MeshBatch> meshes;

        void sort() {
            if (batch_type == RENDERBATCH_TYPE_OPAQUE) {
                for (auto &mesh_batch : meshes) {
                    std::sort(mesh_batch.mesh_draw_infos.begin(), mesh_batch.mesh_draw_infos.end(), [](const MeshDrawInfo &left, const MeshDrawInfo &right) {
                        return left.distance_to_camera < right.distance_to_camera;
                    });
                }
            } else {
                for (auto &mesh_batch : meshes) {
                    std::sort(mesh_batch.mesh_draw_infos.begin(), mesh_batch.mesh_draw_infos.end(), [](const MeshDrawInfo &left, const MeshDrawInfo &right) {
                        return left.distance_to_camera > right.distance_to_camera;
                    });
                }
            }
        }
    };

    struct DrawBatchGenerator {
        static void CreateBatch(const Scene *scene, const Frustum *frustum, const glm::vec3 &camera_position, std::vector<RenderBatch> &mesh_batches, uint32_t batch_filter_flags);

        // Used for Shadow/Cascaded shadow rendering where scene needs to be culled again
        static void CreateShadowMeshBatch(const Scene *scene, const Frustum *frustum, std::vector<ShadowMeshBatch> &render_batches, uint32_t batch_filter_flags);
    };

    void DrawBatch(CommandBuffer *command_buffer, MeshBatch *batch, Shader *shader, uint32_t draw_data_set_id = 4);
} // namespace mirai
