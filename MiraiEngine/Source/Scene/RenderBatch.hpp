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
        RENDERBATCH_TYPE_OPAQUE = 0,
        RENDERBATCH_TYPE_TRANSPARENT,
    };

    struct MeshDrawInfo {
        DrawIndexedIndirectCommand draw_info;
        uint32_t material_index;
        uint32_t transform_index;
        // Distance from camera, used for sorting
        float distance_to_camera;

        MeshDrawInfo(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, float distance_to_camera) : transform_index(transform_index), material_index(material_index), draw_info{
                                                                                                                                                                                                                                               index_count,
                                                                                                                                                                                                                                               1,
                                                                                                                                                                                                                                               index_offset,
                                                                                                                                                                                                                                               vertex_offset,
                                                                                                                                                                                                                                               0},
                                                                                                                                                                         distance_to_camera(distance_to_camera) {
        }
    };

    struct MeshBatch {
        BufferView vertex_buffer;
        BufferView index_buffer;

        // These are currently populated by renderer all at once for all batches
        // We can also move it inside this call
        BufferView transform_buffer_view;
        BufferView material_buffer_view;
        // Only applied for draw indirect call
        BufferView draw_indirect_buffer_view;

        UniformSetID vertex_binding_set;

        std::vector<MeshDrawInfo> mesh_draw_infos;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count, float distance_to_camera) {
            mesh_draw_infos.emplace_back(transform_index, material_index, vertex_offset, index_offset, index_count, distance_to_camera);
        }
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
        static void CreateBatch(const Scene *scene, const Frustum *frustum, const glm::vec3 &camera_position, std::vector<RenderBatch> &render_batches, bool only_opaque);
    };

    void DrawBatch(CommandBuffer *command_buffer, MeshBatch *batch, Shader *shader);
} // namespace mirai
