#include "RenderBatch.hpp"

#include "Material.hpp"
#include "Component.hpp"
#include "Scene.hpp"
#include "Common/JobSystem.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"

#include <mutex>

namespace mirai {

    void DrawBatchGenerator::BuildBatches(Scene *scene, const BatchBuildParams &build_params, std::vector<RenderBatch> &batches) {

        uint32_t total_renderables = scene->render_object_count.load();
        if (total_renderables == 0)
            return;

        const auto &renderables = scene->render_object_list;
        const FrustumPlanes *frustum = build_params.frustum;
        bool is_shadow_pass = build_params.pass == PASS_MODE_DIRLIGHT_SHADOW;

        // Collect all the sort key in first pass
        std::vector<uint64_t> renderable_sort_keys(total_renderables);
        jobsystem::Dispatch(total_renderables, 64, [&](jobsystem::JobDispatchArg arg) {
            uint32_t index = arg.job_index;
            const RenderableObjectData &renderable = renderables[index];
            const auto &material = scene->materials[renderable.material_index];

            // Skip transparent object and object that doesn't cast shadow
            if (is_shadow_pass) {
                if (material->is_transparent()) {
                    renderable_sort_keys[index] = UINT64_MAX;
                    return;
                }
            }

            // Do frustum culling
            if (frustum && !frustum->intersect_aabb(renderable.transformed_aabb)) {
                renderable_sort_keys[index] = UINT64_MAX;
                return;
            }

            renderable_sort_keys[index] = create_sort_key(build_params.pass, material->get_hash(build_params.pass), renderable.mesh_type, renderable.buffer);
        });

        jobsystem::Wait();

        // Dedup and list unique keys
        std::vector<uint64_t> sorted_keys = renderable_sort_keys;
        std::sort(sorted_keys.begin(), sorted_keys.end());

        std::vector<std::pair<uint64_t, int>> batch_infos;
        batch_infos.reserve(25);

        for (uint32_t i = 0; i < total_renderables;) {
            uint64_t sort_key = sorted_keys[i];
            if (sort_key == UINT64_MAX)
                break;
            uint32_t last_index = cast_u32(
                std::upper_bound(sorted_keys.begin() + i, sorted_keys.end(), sort_key) - sorted_keys.begin());

            uint32_t total_item = last_index - i;
            batch_infos.push_back(std::make_pair(sort_key, total_item));
            i = last_index;
        }

        uint32_t batch_count = cast_u32(batch_infos.size());
        HashMap<uint64_t, uint32_t> sort_key_to_batch_index;
        std::vector<std::atomic<int>> batch_counters(batch_count);

        batches.clear();
        batches.resize(batch_count);
        // Initialize sort key for baatches
        for (uint32_t i = 0; i < batches.size(); ++i) {
            auto [sort_key, total_item] = batch_infos[i];
            batches[i].sort_key = sort_key;
            batches[i].draw_infos.resize(total_item);
            batch_counters[i].store(0, std::memory_order_relaxed);
            sort_key_to_batch_index.insert(std::make_pair(sort_key, i));
        }

        jobsystem::Dispatch(total_renderables, 64, [&](jobsystem::JobDispatchArg arg) {
            uint32_t index = arg.job_index;
            uint64_t sort_key = renderable_sort_keys[index];
            if (sort_key == UINT64_MAX)
                return;

            const RenderableObjectData &renderable = renderables[index];

            float distance_to_cam_sqr = 0;
            if (build_params.camera_position) {
                glm::vec3 delta = *build_params.camera_position - renderable.transformed_aabb.get_center();
                distance_to_cam_sqr = glm::dot(delta, delta);
            }

            uint32_t batch_index = sort_key_to_batch_index.at(sort_key);
            uint32_t slot_index = batch_counters[batch_index].fetch_add(1, std::memory_order_relaxed);
            batches[batch_index].draw_infos[slot_index] = {
                .transform_index = renderable.transform_index,
                .material_index = renderable.material_index,
                .draw_info = {
                    .index_count = renderable.index_count,
                    .instance_count = 1,
                    .first_index = renderable.first_index,
                    .vertex_offset_bytes = renderable.vertex_offset_bytes,
                    .first_instance = 0,
                },
                .vertex_stride = renderable.vertex_stride,
                .distance_to_camera_sqr = distance_to_cam_sqr,
            };
        });
        jobsystem::Wait();

        // Can run this in thread as well
        for (auto &batch : batches)
            batch.sort();
    }

    static void DrawBatchIndirect(CommandBuffer *command_buffer, const RenderBatch *batch) {
        BufferID buffer = batch->get_geometry_buffer();
        // Should check if this buffer is same as last buffer
        command_buffer->set_index_buffer(buffer);
        uint32_t draw_count = batch->draw_indirect_buffer_view.size / sizeof(DrawIndexedIndirectCommand);
        command_buffer->draw_indexed_indirect(batch->draw_indirect_buffer_view.buffer, batch->draw_indirect_buffer_view.offset, draw_count, sizeof(DrawIndexedIndirectCommand));
    }

    static void _DrawBatch(CommandBuffer *command_buffer, const RenderBatch *batch, DrawMode draw_mode) {
        switch (draw_mode) {
        case DRAWMODE_INDEXED_INDIRECT:
            DrawBatchIndirect(command_buffer, batch);
            return;

        default:
            Log::Fatal(0, "Undefined shader draw mode");
        }
    }

    void DrawBatch(CommandBuffer *command_buffer, const RenderBatch &render_batch, const BatchDrawInfo &batch_info) {
        if (render_batch.draw_infos.size() == 0)
            return;

        ASSERT(batch_info.shader != nullptr);
        Shader *shader = batch_info.shader;

        command_buffer->bind_pipeline(shader->pipeline_id);

        uint32_t descriptor_push_index_offset = 0;
        // We have only one push constant, so we can ignore the offset which should be always zero
        if (batch_info.push_data != nullptr) {
            command_buffer->set_push_data(batch_info.push_data->offset, batch_info.push_data->data, batch_info.push_data->size);
            descriptor_push_index_offset = batch_info.push_data->size;
        }

        command_buffer->set_push_data(descriptor_push_index_offset, batch_info.descriptor_infos.data(), cast_u32(batch_info.descriptor_infos.size() * sizeof(uint32_t)));

        _DrawBatch(command_buffer, &render_batch, shader->get_draw_mode());
    }
} // namespace mirai