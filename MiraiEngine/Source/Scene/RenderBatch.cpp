#include "RenderBatch.hpp"
#include "Scene/Scene.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    struct CachedBatchInfo {
        RenderBatchType batch_type;
        uint64_t material_hash;
        BufferView vertex_buffer;
    };

    uint32_t FindOrCreateShaderBatch(uint64_t material_hash, RenderBatchType render_batch_type, std::vector<RenderBatch> &render_batches) {
        for (uint32_t i = 0; i < render_batches.size(); ++i) {
            if (material_hash == render_batches[i].material_hash && render_batches[i].batch_type == render_batch_type)
                return i;
        }
        render_batches.emplace_back(material_hash, render_batch_type);
        return cast_u32(render_batches.size() - 1);
    }

    uint32_t FindOrCreateMeshBatch(BufferView vertex_buffer, BufferView index_buffer, std::vector<MeshBatch> &mesh_batches) {
        for (uint32_t i = 0; i < mesh_batches.size(); ++i) {
            // No need to check index buffer for now
            if (mesh_batches[i].vertex_buffer.buffer == vertex_buffer.buffer && mesh_batches[i].index_buffer.buffer == index_buffer.buffer)
                return i;
        }
        mesh_batches.push_back(MeshBatch{
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
        });
        return cast_u32(mesh_batches.size() - 1);
    }

    uint32_t FindOrCreateShadowMeshBatch(BufferView vertex_buffer, BufferView index_buffer, std::vector<ShadowMeshBatch> &mesh_batches, RenderBatchType render_batch_type) {
        for (uint32_t i = 0; i < mesh_batches.size(); ++i) {
            // No need to check index buffer for now
            if (mesh_batches[i].vertex_buffer.buffer == vertex_buffer.buffer && mesh_batches[i].index_buffer.buffer == index_buffer.buffer && mesh_batches[i].render_batch_type == render_batch_type)
                return i;
        }
        mesh_batches.push_back(ShadowMeshBatch{
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
            .render_batch_type = render_batch_type,
        });
        return cast_u32(mesh_batches.size() - 1);
    }

    void DrawBatchGenerator::CreateBatch(const Scene *scene, const Frustum *frustum, const glm::vec3 &camera_position, std::vector<RenderBatch> &render_batches, uint32_t batch_filter_flags) {
        auto &render_object_list = scene->render_object_list;
        CachedBatchInfo cached_batch_info = {
            .batch_type = RENDERBATCH_TYPE_OPAQUE,
            .vertex_buffer = BufferID{K_INVALID_ID},
        };
        cached_batch_info.material_hash = UINT64_MAX;

        uint32_t shader_batch = UINT32_MAX;
        uint32_t mesh_batch = UINT32_MAX;

        bool skip_near_plane = (batch_filter_flags & BATCH_FILTER_SKIP_NEAR_PLANE) == BATCH_FILTER_SKIP_NEAR_PLANE;
        auto &component_manager = scene->ecs->component_manager;
        for (auto &object : render_object_list) {
            const Material3D *material = scene->materials[object.material_index].get();

            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            uint32_t filter_flag = BATCH_FILTER_FLAG_OPAQUE;
            if (material->is_transparent()) {
                render_batch_type = RENDERBATCH_TYPE_TRANSPARENT;
                filter_flag = BATCH_FILTER_FLAG_TRANSPARENT;
            } else if (material->is_alpha_mask()) {
                render_batch_type = RENDERBATCH_TYPE_ALPHA_MASK;
                filter_flag = BATCH_FILTER_FLAG_ALPHA_MASK;
            }

            if ((object.render_flags & MeshComponent::FLAGS::SKINNED) == MeshComponent::FLAGS::SKINNED) {
                render_batch_type = RENDERBATCH_TYPE_SKINNED;
                filter_flag = BATCH_FILTER_FLAG_SKINNED;
            }

            if ((batch_filter_flags & filter_flag) != filter_flag)
                continue;

            // Check if the AABB is visible or not in current frustum
            bool disable_frustum_culling = (object.render_flags & MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING) == MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING;
            if (!disable_frustum_culling && frustum != nullptr) {
                // Cache AABB Transform
                if (!frustum->intersect(object.transformed_aabb, skip_near_plane))
                    continue;
            }

            // Check Shader Batch
            uint64_t material_hash = material->get_hash();
            if (material_hash != cached_batch_info.material_hash || cached_batch_info.batch_type != render_batch_type) {
                // We have a different batch
                shader_batch = FindOrCreateShaderBatch(material_hash, render_batch_type, render_batches);
                cached_batch_info.material_hash = material_hash;
                cached_batch_info.batch_type = render_batch_type;
                // Reset buffer info
                cached_batch_info.vertex_buffer.buffer = BufferID{K_INVALID_ID};
            }

            // Check Mesh Batch
            if (cached_batch_info.vertex_buffer != object.vertex_buffer) {
                mesh_batch = FindOrCreateMeshBatch(object.vertex_buffer, object.index_buffer, render_batches[shader_batch].meshes);
                cached_batch_info.vertex_buffer = object.vertex_buffer;
            }

            TransformComponent *transform = component_manager->get_component<TransformComponent>(object.entity);
            float distance_to_camera = glm::dot(transform->position, camera_position);
            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(object.entity);
            render_batches[shader_batch].meshes[mesh_batch].add(transform_index, object.material_index, object.vertex_offset_bytes, object.first_index, object.index_count, distance_to_camera, object.vertex_stride);
        }
    }

    void DrawBatchGenerator::CreateShadowMeshBatch(const Scene *scene, const Frustum *frustum, std::vector<ShadowMeshBatch> &mesh_batches, uint32_t batch_filter_flags) {
        auto &render_object_list = scene->render_object_list;

        BufferView cached_vertex_buffer = BufferView{BufferID{K_INVALID_ID}, 0, 0};
        uint32_t mesh_batch = UINT32_MAX;
        RenderBatchType last_render_batch_type = RENDERBATCH_TYPE_OPAQUE;

        auto &component_manager = scene->ecs->component_manager;
        for (auto &object : render_object_list) {
            const Material3D *material = scene->materials[object.material_index].get();
            uint32_t filter_flag = BATCH_FILTER_FLAG_OPAQUE;
            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            if (material->is_transparent()) {
                filter_flag |= BATCH_FILTER_FLAG_TRANSPARENT;
                render_batch_type = RENDERBATCH_TYPE_TRANSPARENT;
            } else if (material->is_alpha_mask()) {
                filter_flag |= BATCH_FILTER_FLAG_ALPHA_MASK;
                render_batch_type = RENDERBATCH_TYPE_ALPHA_MASK;
            }

            if ((batch_filter_flags & filter_flag) != filter_flag)
                continue;

            bool skip_near_plane = (batch_filter_flags & BATCH_FILTER_SKIP_NEAR_PLANE) == BATCH_FILTER_SKIP_NEAR_PLANE;
            bool disable_frustum_culling = (object.render_flags & MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING) == MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING;
            if (!disable_frustum_culling && frustum != nullptr) {
                if (!frustum->intersect(object.transformed_aabb, skip_near_plane))
                    continue;
            }

            // Check Mesh Batch
            if (cached_vertex_buffer != object.vertex_buffer || last_render_batch_type != render_batch_type) {
                mesh_batch = FindOrCreateShadowMeshBatch(object.vertex_buffer, object.index_buffer, mesh_batches, render_batch_type);
                cached_vertex_buffer = object.vertex_buffer;
                last_render_batch_type = render_batch_type;
            }

            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(object.entity);
            mesh_batches[mesh_batch].add(transform_index, object.material_index, object.vertex_offset_bytes, object.first_index, object.index_count, object.vertex_stride);
        }
    }

    void DrawBatchIndirect(CommandBuffer *command_buffer, const MeshBatch *batch) {
        command_buffer->set_index_buffer(batch->index_buffer.buffer);
        uint32_t draw_count = batch->draw_indirect_buffer_view.size / sizeof(DrawIndexedIndirectCommand);
        command_buffer->draw_indexed_indirect(batch->draw_indirect_buffer_view.buffer, batch->draw_indirect_buffer_view.offset, draw_count, sizeof(DrawIndexedIndirectCommand));
    }

    void _DrawBatch(CommandBuffer *command_buffer, const MeshBatch *batch, DrawMode draw_mode) {
        switch (draw_mode) {
        case DRAWMODE_INDEXED_INDIRECT:
            DrawBatchIndirect(command_buffer, batch);
            return;

        default:
            Log::Fatal(0, "Undefined shader draw mode");
        }
    }

    void DrawBatch(CommandBuffer *command_buffer, const std::vector<RenderBatch> &render_batches, const BatchDrawInfo &batch_info) {
        for (auto &batch : render_batches) {
            if (batch.batch_type != batch_info.batch_type)
                continue;

            ASSERT(batch_info.shader != nullptr);
            Shader *shader = batch_info.shader;

            command_buffer->bind_pipeline(shader->pipeline_id);

            uint32_t descriptor_push_index_offset = 0;
            // We have only one push constant, so we can ignore the offset which should be always zero
            if (batch_info.push_constants != nullptr) {
                command_buffer->set_push_data(batch_info.push_constants->offset, batch_info.push_constants->data, batch_info.push_constants->size);
                descriptor_push_index_offset = batch_info.push_constants->size;
            }

            Renderer *renderer = Renderer::get();
            DescriptorInfo mesh_data_descriptor_info = {.type = DescriptorType::StorageBuffer};
            for (const auto &mesh_batch : batch.meshes) {
                if (mesh_batch.mesh_draw_infos.size() == 0)
                    continue;
                // We are copying data, yes
                mesh_data_descriptor_info.resource = mesh_batch.draw_data_buffer_view.buffer;
                mesh_data_descriptor_info.offset = mesh_batch.draw_data_buffer_view.offset;
                mesh_data_descriptor_info.size = mesh_batch.draw_data_buffer_view.size;

                std::vector<DescriptorOffset> descriptors = batch_info.descriptor_infos;
                descriptors.push_back(renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &mesh_data_descriptor_info, 1));
                command_buffer->set_push_data(descriptor_push_index_offset, descriptors.data(), cast_u32(descriptors.size() * sizeof(uint32_t)));

                _DrawBatch(command_buffer, &mesh_batch, shader->get_draw_mode());
            }
        }
    }
} // namespace mirai