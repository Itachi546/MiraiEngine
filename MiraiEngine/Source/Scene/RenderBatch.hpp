#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Material.hpp"
#include <vector>
#include <Math/Math.hpp>

namespace mirai {
    class Scene;
    class CommandBuffer;

    enum RenderBatchType {
        RENDERBATCH_TYPE_OPAQUE = 0,
        RENDERBATCH_TYPE_TRANSPARENT,
    };

    struct MeshBatch {
        BufferView vertex_buffer;
        BufferView index_buffer;

        // These are currently populated by renderer all at once for all batches
        // We can also move it inside this call
        BufferView transform_buffer_view;
        BufferView material_buffer_view;

        UniformSetID vertex_binding_set;

        std::vector<uint32_t> transform_indices;
        std::vector<uint32_t> material_indices;
        std::vector<uint32_t> vertex_offsets;
        std::vector<uint32_t> index_offsets;
        std::vector<uint32_t> index_counts;

        void add(uint32_t transform_index, uint32_t material_index, uint32_t vertex_offset, uint32_t index_offset, uint32_t index_count) {
            transform_indices.push_back(transform_index);
            material_indices.push_back(material_index);
            vertex_offsets.push_back(vertex_offset);
            index_offsets.push_back(index_offset);
            index_counts.push_back(index_count);
        }
    };

    struct RenderBatch {
        ShaderPassKey shader_key;
        RenderBatchType batch_type;
        std::vector<MeshBatch> meshes;
    };
    struct DrawBatchGenerator {
        static void CreateBatch(const Scene *scene, const Frustum *frustum, std::vector<RenderBatch> &render_batches, bool only_opaque);
    };

    void DrawBatch(CommandBuffer *command_buffer, MeshBatch *batch, Shader *shader);
} // namespace mirai