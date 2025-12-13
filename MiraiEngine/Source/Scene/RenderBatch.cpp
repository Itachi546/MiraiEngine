#include "RenderBatch.hpp"
#include "Scene/Scene.hpp"

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

    void DrawBatchGenerator::CreateBatch(const Scene *scene, const Frustum *frustum, std::vector<RenderBatch> &render_batches, bool only_opaque) {
        auto &render_object_list = scene->render_object_list;
        CachedBatchInfo cached_batch_info = {
            .batch_type = RENDERBATCH_TYPE_OPAQUE,
            .vertex_buffer = BufferID{K_INVALID_ID},
        };
        cached_batch_info.shader_key.key = UINT64_MAX;

        uint32_t shader_batch = UINT32_MAX;
        uint32_t mesh_batch = UINT32_MAX;

        for (auto &object : render_object_list) {
            const Material *material = scene->materials[object.material_index].get();

            // Check if the AABB is visible or not in current frustum
            TransformComponent *transform = scene->component_manager->get_component<TransformComponent>(object.entity);
            AABB aabb = object.aabb;
            aabb.transform(transform->world_transform);
            if (!frustum->intersect(aabb))
                continue;

            RenderBatchType render_batch_type = RENDERBATCH_TYPE_OPAQUE;
            if (material->is_transparent()) {
                if (only_opaque)
                    continue;
                render_batch_type = RENDERBATCH_TYPE_TRANSPARENT;
            }

            // Check Shader Batch
            ShaderPassKey shader_key = material->get_shader_key();
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

            uint32_t transform_index = scene->component_manager->get_component_index<TransformComponent>(object.entity);
            render_batches[shader_batch].meshes[mesh_batch].add(transform_index, object.material_index, object.vertex_offset, object.index_offset, object.index_count);
        }
    }
} // namespace mirai