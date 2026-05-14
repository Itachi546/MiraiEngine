#include "RenderBatch.hpp"
#include "Scene/Scene.hpp"
#include "Math/Frustum.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {
    // ── Internal helpers ─────────────────────────────────────────────────────
    inline uint32_t compute_sort_key(const MaterialOverrides *override_state,
                                     const Material *material,
                                     MeshType mesh_type) {
        if (override_state == nullptr) {
            uint32_t hash = material->get_hash();
            return hash << 16 | mesh_type;
        }

        uint32_t hash = material->get_hash();
        hash = (hash & ~override_state->override_flag) | override_state->override_value;
        return (hash << 16) | mesh_type;
    }

    struct CachedBatchInfo {
        RenderBatchType batch_type;
        uint32_t sort_key;
        BufferID vertex_buffer;

        void update_cache_info(RenderBatchType batch_type, uint32_t sort_key, BufferID vertex_buffer) {
            this->batch_type = batch_type;
            this->sort_key = sort_key;
            this->vertex_buffer = vertex_buffer;
        }
    };

    static uint32_t FindOrCreateShaderBatch(uint32_t sort_key, RenderBatchType render_batch_type, std::vector<RenderBatch> &render_batches) {
        for (uint32_t i = 0; i < render_batches.size(); ++i) {
            if (sort_key == render_batches[i].sort_key && render_batches[i].batch_type == render_batch_type)
                return i;
        }
        render_batches.emplace_back(sort_key, render_batch_type);
        return cast_u32(render_batches.size() - 1);
    }

    static uint32_t FindOrCreateMeshBatch(BufferID vertex_buffer, BufferID index_buffer, std::vector<MeshBatch> &mesh_batches) {
        for (uint32_t i = 0; i < mesh_batches.size(); ++i) {
            if (mesh_batches[i].vertex_buffer == vertex_buffer && mesh_batches[i].index_buffer == index_buffer)
                return i;
        }
        mesh_batches.push_back(MeshBatch{
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
        });
        return cast_u32(mesh_batches.size() - 1);
    }

    void DrawBatchGenerator::BuildBatches(const Scene *scene,
                                          const BatchBuildParams &params,
                                          std::vector<RenderBatch> &render_batches) {

        CachedBatchInfo cached_batch_info = {
            .batch_type = RENDERBATCH_TYPE_OPAQUE,
            .sort_key = 0,
            .vertex_buffer = BufferID{K_INVALID_ID},
        };

        uint32_t shader_batch = UINT32_MAX;
        uint32_t mesh_batch = UINT32_MAX;

        auto &component_manager = scene->ecs->component_manager;

        uint32_t render_object_count = scene->render_object_count.load(std::memory_order_relaxed);
        for (uint32_t i = 0; i < render_object_count; ++i) {
            const RenderableObjectData &object = scene->render_object_list[i];

            const Material3D *material = scene->materials[object.material_index].get();

            if (params.shadow_pass) {
                if (!material->has_render_flag(RENDER_FLAG_CAST_SHADOW))
                    continue;
                if (material->is_transparent())
                    continue;
            }

            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            uint32_t filter_flag = BATCH_FILTER_FLAG_OPAQUE;

            if (material->is_transparent()) {
                render_batch_type = RENDERBATCH_TYPE_TRANSPARENT;
                filter_flag = BATCH_FILTER_FLAG_TRANSPARENT;
            } else if (material->is_alpha_mask()) {
                render_batch_type = RENDERBATCH_TYPE_ALPHA_MASK;
                filter_flag = BATCH_FILTER_FLAG_ALPHA_MASK;
            }
            /*
            if (object.mesh_type == MESH_TYPE_SKINNED) {
                render_batch_type = RENDERBATCH_TYPE_SKINNED;
                filter_flag = BATCH_FILTER_FLAG_SKINNED;
            }
            */
            if ((params.filter_flags & filter_flag) != filter_flag)
                continue;

            if (params.frustum != nullptr || render_batch_type) {
                if (!params.frustum->intersect_aabb(object.transformed_aabb))
                    continue;
            }

            // Sort key — standard PBR uses material state hash; ShaderMaterial3D uses pipeline_id.
            Shader *object_custom_shader = nullptr;
            uint32_t sort_key;

            if (material->is_custom_shader()) {
                object_custom_shader = material->get_custom_shader();
                sort_key = make_custom_sort_key(object_custom_shader->pipeline_id.id);
            } else {
                sort_key = compute_sort_key(params.pass_state_override, material, object.mesh_type);
            }

            if (sort_key != cached_batch_info.sort_key ||
                cached_batch_info.batch_type != render_batch_type ||
                shader_batch == UINT32_MAX) {
                shader_batch = FindOrCreateShaderBatch(sort_key, render_batch_type, render_batches);
                if (object_custom_shader != nullptr)
                    render_batches[shader_batch].custom_shader = object_custom_shader;
                cached_batch_info.update_cache_info(render_batch_type, sort_key, BufferID{K_INVALID_ID});
            }

            if (cached_batch_info.vertex_buffer != object.vertex_buffer) {
                mesh_batch = FindOrCreateMeshBatch(object.vertex_buffer, object.index_buffer, render_batches[shader_batch].meshes);
                cached_batch_info.vertex_buffer = object.vertex_buffer;
            }

            float distance_to_camera_sqr = 0.0f;
            if (params.camera_position != nullptr) {
                TransformComponent *transform = component_manager->get_component<TransformComponent>(object.entity);
                distance_to_camera_sqr = glm::dot(transform->position, *params.camera_position);
            }

            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(object.entity);
            render_batches[shader_batch].meshes[mesh_batch].add(
                transform_index,
                object.material_index,
                object.vertex_offset_bytes,
                object.first_index,
                object.index_count,
                distance_to_camera_sqr,
                object.vertex_stride);
        }
    }

    static void DrawBatchIndirect(CommandBuffer *command_buffer, const MeshBatch *batch) {
        command_buffer->set_index_buffer(batch->index_buffer);
        uint32_t draw_count = batch->draw_indirect_buffer_view.size / sizeof(DrawIndexedIndirectCommand);
        command_buffer->draw_indexed_indirect(batch->draw_indirect_buffer_view.buffer, batch->draw_indirect_buffer_view.offset, draw_count, sizeof(DrawIndexedIndirectCommand));
    }

    static void _DrawBatch(CommandBuffer *command_buffer, const MeshBatch *batch, DrawMode draw_mode) {
        switch (draw_mode) {
        case DRAWMODE_INDEXED_INDIRECT:
            DrawBatchIndirect(command_buffer, batch);
            return;

        default:
            Log::Fatal(0, "Undefined shader draw mode");
        }
    }

    void DrawBatch(CommandBuffer *command_buffer, const RenderBatch &render_batch, const BatchDrawInfo &batch_info) {

        ASSERT(batch_info.shader != nullptr);
        Shader *shader = batch_info.shader;

        command_buffer->bind_pipeline(shader->pipeline_id);

        uint32_t descriptor_push_index_offset = 0;
        // We have only one push constant, so we can ignore the offset which should be always zero
        if (batch_info.push_data != nullptr) {
            command_buffer->set_push_data(batch_info.push_data->offset, batch_info.push_data->data, batch_info.push_data->size);
            descriptor_push_index_offset = batch_info.push_data->size;
        }

        Renderer *renderer = Renderer::get();
        DescriptorInfo mesh_data_descriptor_info = {.type = DescriptorType::StorageBuffer};
        for (const auto &mesh_batch : render_batch.meshes) {
            if (mesh_batch.mesh_draw_infos.size() == 0)
                continue;
            // We are copying data, yes
            mesh_data_descriptor_info.resource = mesh_batch.draw_data_buffer_view.buffer;
            mesh_data_descriptor_info.buffer_info.offset = mesh_batch.draw_data_buffer_view.offset;
            mesh_data_descriptor_info.buffer_info.size = mesh_batch.draw_data_buffer_view.size;

            std::vector<DescriptorOffset> descriptors = batch_info.descriptor_infos;
            descriptors[batch_info.draw_data_descriptor_index] = renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &mesh_data_descriptor_info, 1);
            command_buffer->set_push_data(descriptor_push_index_offset, descriptors.data(), cast_u32(descriptors.size() * sizeof(uint32_t)));

            _DrawBatch(command_buffer, &mesh_batch, shader->get_draw_mode());
        }
    }
} // namespace mirai