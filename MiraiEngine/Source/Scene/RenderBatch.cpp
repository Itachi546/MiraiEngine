#include "RenderBatch.hpp"
#include "Scene/Scene.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/Renderer.hpp"
#include <unordered_map>

namespace mirai {

    struct CachedBatchInfo {
        RenderBatchType batch_type;
        ShaderPassKey shader_key;
        BufferView vertex_buffer;
    };

    uint32_t FindOrCreateShaderBatch(ShaderPassKey shader_key, RenderBatchType render_batch_type, std::vector<RenderBatch> &render_batches) {
        for (uint32_t i = 0; i < render_batches.size(); ++i) {
            if (shader_key == render_batches[i].shader_key)
                return i;
        }
        render_batches.emplace_back(shader_key, render_batch_type);
        return cast_u32(render_batches.size() - 1);
    }

    uint32_t FindOrCreateMeshBatch(BufferView vertex_buffer, BufferView index_buffer, UniformSetID vertex_binding_set, std::vector<MeshBatch> &mesh_batches) {
        for (uint32_t i = 0; i < mesh_batches.size(); ++i) {
            // No need to check index buffer for now
            if (mesh_batches[i].vertex_buffer.buffer == vertex_buffer.buffer && mesh_batches[i].index_buffer.buffer == index_buffer.buffer)
                return i;
        }
        mesh_batches.push_back(MeshBatch{
            .vertex_buffer = vertex_buffer,
            .index_buffer = index_buffer,
            .vertex_binding_set = vertex_binding_set,
        });
        return cast_u32(mesh_batches.size() - 1);
    }

    void DrawBatchGenerator::CreateBatch(const Scene *scene, const Frustum *frustum, const glm::vec3 &camera_position, std::vector<RenderBatch> &render_batches, RenderMode render_mode, bool only_opaque) {
        auto &render_object_list = scene->render_object_list;
        CachedBatchInfo cached_batch_info = {
            .batch_type = RENDERBATCH_TYPE_OPAQUE,
            .vertex_buffer = BufferID{K_INVALID_ID},
        };
        cached_batch_info.shader_key.key = UINT64_MAX;

        uint32_t shader_batch = UINT32_MAX;
        uint32_t mesh_batch = UINT32_MAX;

        auto &component_manager = scene->ecs->component_manager;
        for (auto &object : render_object_list) {
            // Check if the AABB is visible or not in current frustum
            bool disable_frustum_culling = (object.render_flags & MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING) == MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING;
            TransformComponent *transform = component_manager->get_component<TransformComponent>(object.entity);
            if (!disable_frustum_culling) {
                AABB aabb = object.aabb;
                aabb.transform(transform->world_transform);
                if (!frustum->intersect(aabb))
                    continue;
            }

            const Material *material = scene->materials[object.material_index].get();
            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            if (material->is_transparent()) {
                if (only_opaque)
                    continue;
                render_batch_type = RENDERBATCH_TYPE_TRANSPARENT;
            }

            // Check Shader Batch
            ShaderPassKey shader_key = material->get_shader_key(render_mode);
            if (shader_key != cached_batch_info.shader_key) {
                // We have a different batch
                shader_batch = FindOrCreateShaderBatch(shader_key, render_batch_type, render_batches);
                cached_batch_info.shader_key = shader_key;
                cached_batch_info.batch_type = render_batch_type;
                // Reset buffer info
                cached_batch_info.vertex_buffer.buffer = BufferID{K_INVALID_ID};
            }

            // Check Mesh Batch
            if (cached_batch_info.vertex_buffer != object.vertex_buffer) {
                mesh_batch = FindOrCreateMeshBatch(object.vertex_buffer, object.index_buffer, object.vertex_binding_set, render_batches[shader_batch].meshes);
                cached_batch_info.vertex_buffer = object.vertex_buffer;
            }
            float distance_to_camera = glm::dot(transform->position, camera_position);
            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(object.entity);
            render_batches[shader_batch].meshes[mesh_batch].add(transform_index, object.material_index, object.vertex_offset, object.index_offset, object.index_count, distance_to_camera);
        }
    }

    void DrawBatchGenerator::CreateMeshBatch(const Scene *scene, const Frustum *frustum, std::vector<MeshBatch> &mesh_batches, bool only_opaque) {
        auto &render_object_list = scene->render_object_list;

        BufferView cached_vertex_buffer = BufferView{BufferID{K_INVALID_ID}, 0, 0};
        uint32_t mesh_batch = UINT32_MAX;

        auto &component_manager = scene->ecs->component_manager;
        for (auto &object : render_object_list) {
            bool disable_frustum_culling = (object.render_flags & MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING) == MeshComponent::FLAGS::DISABLE_FRUSTUM_CULLING;
            TransformComponent *transform = component_manager->get_component<TransformComponent>(object.entity);
            if (!disable_frustum_culling) {
                AABB aabb = object.aabb;
                aabb.transform(transform->world_transform);
                if (!frustum->intersect(aabb))
                    continue;
            }

            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            const Material *material = scene->materials[object.material_index].get();
            if (material->is_transparent()) {
                if (only_opaque)
                    continue;
            }

            // Check Mesh Batch
            if (cached_vertex_buffer != object.vertex_buffer) {
                mesh_batch = FindOrCreateMeshBatch(object.vertex_buffer, object.index_buffer, object.vertex_binding_set, mesh_batches);
                cached_vertex_buffer = object.vertex_buffer;
            }
            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(object.entity);
            mesh_batches[mesh_batch].add(transform_index, object.material_index, object.vertex_offset, object.index_offset, object.index_count, 0.0f);
        }
    }

    static UniformLayout DRAW_DATA_LAYOUT = {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX};

    void DrawBatchIndirect(CommandBuffer *command_buffer, MeshBatch *batch, PipelineID pipeline_id, uint32_t draw_data_set_id) {
        if (batch->mesh_draw_infos.size() == 0)
            return;

        RenderingDevice *device = RenderingDevice::get();

        UniformSetID draw_data_set = command_buffer->create_uniform_set(&DRAW_DATA_LAYOUT, 1, draw_data_set_id);
        UniformBinding draw_data_binding = {
            .resource_id = batch->draw_data_buffer_view.buffer,
            .buffer_info{
                .offset = batch->draw_data_buffer_view.offset,
                .range = batch->draw_data_buffer_view.size,
            },
        };
        device->update_uniform_set(draw_data_set, &draw_data_binding, 1);

        UniformSetID uniform_sets[] = {batch->vertex_binding_set, draw_data_set};
        command_buffer->set_uniform_sets(pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
        command_buffer->set_index_buffer(batch->index_buffer.buffer);

        uint32_t draw_count = batch->draw_indirect_buffer_view.size / sizeof(DrawIndexedIndirectCommand);
        command_buffer->draw_indexed_indirect(batch->draw_indirect_buffer_view.buffer, batch->draw_indirect_buffer_view.offset, draw_count, sizeof(DrawIndexedIndirectCommand));
    }

    void DrawBatch(CommandBuffer *command_buffer, MeshBatch *batch, Shader *shader, uint32_t draw_data_set_id) {
        switch (shader->get_draw_mode()) {
        case DRAWMODE_INDEXED_INDIRECT:
            DrawBatchIndirect(command_buffer, batch, shader->pipeline_id, draw_data_set_id);
            return;

        default:
            Log::Fatal(0, "Undefined shader draw mode");
        }
    }
} // namespace mirai