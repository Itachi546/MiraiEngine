#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <vector>
#include <Math/Math.hpp>

namespace mirai {
    class Scene;

    enum RenderBatchType {
        RENDERBATCH_TYPE_OPAQUE = 0,
        RENDERBATCH_TYPE_TRANSPARENT,
    };

    struct RenderBatch {
        BufferID vertex_buffer;
        BufferID index_buffer;
        UniformSetID vertex_binding_set;
        RenderBatchType batch_type;

        // Optional Buffers
        BufferView transform_buffer_view;
        BufferView material_buffer_view;

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

    struct DrawBatchGenerator {
        static void CreateBatch(const Scene *scene, const Frustum *frustum, std::vector<RenderBatch> &render_batches, bool only_opaque);
    };
} // namespace mirai