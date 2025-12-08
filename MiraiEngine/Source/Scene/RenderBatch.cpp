#include "RenderBatch.hpp"

#include "Scene/Scene.hpp"
#include <unordered_map>

namespace mirai {
    struct BatchGenerateInfo {
        uint32_t batch_index;
        uint32_t buffer_id;
        RenderBatchType batch_type;
    };

    uint32_t FindRenderBatch(BufferID buffer_id, std::vector<RenderBatch> &render_batches, RenderBatchType render_batch_type) {
        for (uint32_t i = 0; i < render_batches.size(); ++i) {
            if (render_batches[i].batch_type == render_batch_type && buffer_id == render_batches[i].vertex_buffer)
                return i;
        }
        return UINT32_MAX;
    }

    void DrawBatchGenerator::CreateBatch(const Scene *scene, const Frustum *frustum, std::vector<RenderBatch> &render_batches, bool only_opaque) {
        auto &render_object_list = scene->render_object_list;
        BatchGenerateInfo last_transparent_batch = {
            .batch_index = UINT32_MAX,
            .buffer_id = K_INVALID_ID,
            .batch_type = RENDERBATCH_TYPE_TRANSPARENT,
        };

        BatchGenerateInfo last_opaque_batch = {
            .batch_index = UINT32_MAX,
            .buffer_id = K_INVALID_ID,
            .batch_type = RENDERBATCH_TYPE_OPAQUE,
        };

        BatchGenerateInfo *target_batch_info = nullptr;

        for (auto &object : render_object_list) {
            BufferID buffer = object.vertex_buffer;
            const Material *material = scene->materials[object.material_index].get();

            // Check if the AABB is visible or not in current frustum
            TransformComponent *transform = scene->component_manager->get_component<TransformComponent>(object.entity);
            AABB aabb = object.aabb;
            aabb.transform(transform->world_transform);
            if (!frustum->intersect(aabb))
                continue;

            if (material->is_transparent()) {
                if (only_opaque)
                    continue;
                target_batch_info = &last_transparent_batch;
            } else {
                target_batch_info = &last_opaque_batch;
            }

            if (target_batch_info->buffer_id != buffer.id) {
                // Check if it is already in cache
                target_batch_info->buffer_id = buffer.id;
                uint32_t found = FindRenderBatch(buffer, render_batches, target_batch_info->batch_type);
                if (found != UINT32_MAX) {
                    target_batch_info->batch_index = found;
                } else {
                    target_batch_info->batch_index = cast_u32(render_batches.size());
                    render_batches.push_back(RenderBatch{
                        .vertex_buffer = buffer,
                        .index_buffer = object.index_buffer,
                        .vertex_binding_set = object.vertex_binding_set,
                        .batch_type = target_batch_info->batch_type});
                }
            }
            uint32_t transform_index = scene->component_manager->get_component_index<TransformComponent>(object.entity);
            render_batches[target_batch_info->batch_index].add(transform_index, object.material_index, object.vertex_offset, object.index_offset, object.index_count);
        }
    }
} // namespace mirai