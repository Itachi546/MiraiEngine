#include "Scene.hpp"
#include "ShaderMaterial.hpp"
#include "Component.hpp"
#include "Camera.hpp"
#include "Device/Window.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"

#include <execution>
#include <algorithm>

namespace mirai {
    Scene::Scene(const std::string &name) : name(name) {
        component_manager = std::make_unique<ComponentManager>();
        component_manager->register_component<NameComponent>();
        component_manager->register_component<HierarchyComponent>();
        component_manager->register_component<MeshComponent>();
        component_manager->register_component<TransformComponent>();

        BufferDescription buffer_desc = {
            .size = static_cast<uint32_t>(K_MAX_ENTITIES * sizeof(glm::mat4)),
            .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        RenderingDevice *device = RenderingDevice::get();

        // Allocate transform buffer
        transform_buffer = device->create_buffer(&buffer_desc, "transform_buffer");
        transform_array = (glm::mat4 *)(device->map_buffer(transform_buffer));

        // Allocate material buffer
        buffer_desc.size = static_cast<uint32_t>(K_MAX_ENTITIES * sizeof(Material));
        material_buffer = device->create_buffer(&buffer_desc, "material_buffer");
        material_array = device->map_buffer(material_buffer);

        buffer_desc = {
            .size = sizeof(FrameData),
            .usage_flags = BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };

        per_frame_data_buffer = device->create_buffer(&buffer_desc, "per_frame_data_buffer");
        per_frame_data_ptr = device->map_buffer(per_frame_data_buffer);
        per_frame_uniform_set = device->create_uniform_set(&layout, 1, 0, "per_frame_uniform_set");

        UniformBinding binding = {
            .resource_id = per_frame_data_buffer,
            .offset = 0,
            .range = sizeof(FrameData),
        };
        device->update_uniform_set(per_frame_uniform_set, &binding, 1);

        camera = std::make_unique<Camera>();
    }

    void Scene::update() {
        ScopedCpuProfiling("Scene Update");

        camera->update();

        update_transform_components();

        update_hierarchy_component();

        update_draw_data();

        // Sort transparent batches
        /*
        glm::vec3 camera_position = camera->position;
        if (transparent_batches.size() > 0) {
            std::sort(std::execution::par_unseq, transparent_batches.begin(), transparent_batches.end(), [camera_position](const TransparentDrawData &lhs, const TransparentDrawData &rhs) {
                return glm::length(lhs.position - camera_position) > glm::length(rhs.position - camera_position);
            });
        }
        */

        uint32_t width, height;
        Window::get()->get_size(&width, &height);

        camera->set_aspect_ratio(float(width) / float(height));
        glm::mat4 P = camera->get_projection_transform();
        glm::mat4 V = camera->get_view_transform();
        glm::mat4 VP = camera->get_view_projection_transform();

        per_frame_data.elapsed_time = Engine::get()->get_elapsed_seconds();
        per_frame_data.P = P;
        per_frame_data.V = V;
        per_frame_data.VP = VP;
        per_frame_data.window_size = glm::vec2((float)width, (float)height);

        std::memcpy(per_frame_data_ptr, &per_frame_data, sizeof(FrameData));
    }

    void Scene::remove_entity_tree(Entity entity) {
        if (component_manager->has_component<HierarchyComponent>(entity)) {
            HierarchyComponent *comp = component_manager->get_component<HierarchyComponent>(entity);
            for (auto child : comp->childrens)
                remove_entity_tree(child);
        }
        ecs::destroy_entity(component_manager.get(), entity);
    }

    void Scene::update_transform_components() {
        auto transform_array_ptr = component_manager->get_component_array<TransformComponent>();
        std::vector<TransformComponent> &transforms = transform_array_ptr->components;
        std::for_each(std::execution::par_unseq,
                      transforms.begin(),
                      transforms.end(),
                      [](TransformComponent &transform) { transform.update_local_transform(); });
    }

    void Scene::update_hierarchy(Entity entity, const glm::mat4 &parent_transform) {
        TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
        if (transform->dirty) {
            transform->world_transform = parent_transform * transform->local_transform;
            transform->dirty = false;

            HierarchyComponent *hierarchy_component = component_manager->get_component<HierarchyComponent>(entity);
            if (hierarchy_component != nullptr) {
                for (auto &child : hierarchy_component->childrens)
                    update_hierarchy(child, transform->world_transform);
            }
        }
    }

    void Scene::update_hierarchy_component() {
        for (auto &entity : entities)
            update_hierarchy(entity, glm::mat4(1.0f));
    }

    void Scene::update_draw_data() {
        ScopedCpuProfiling("Update Draw Data");

        memcpy(material_array, materials.data(), sizeof(Material) * materials.size());

        opaque_batches.clear();
        transparent_batches.clear();

        auto mesh_component_ptr = component_manager->get_component_array<MeshComponent>();
        std::vector<Entity> &entities = mesh_component_ptr->entities;
        uint32_t component_count = static_cast<uint32_t>(mesh_component_ptr->size());

        DrawData draw_data;
        uint32_t total_entities = 0;

        Frustum &frustum = camera->get_frustum();
        for (uint32_t i = 0; i < component_count; ++i) {
            total_entities++;
            const MeshComponent &mesh_component = mesh_component_ptr->components[i];
            const Entity entity = mesh_component_ptr->entities[i];
            const TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);

            transform_array[i] = transform->world_transform;

            GpuMesh &gpu_mesh = gpu_meshes[mesh_component.gpu_mesh_index];
            draw_data.vertex_buffer = gpu_mesh.vertex_buffer;
            draw_data.index_buffer = gpu_mesh.index_buffer;
            draw_data.vertex_binding_set = gpu_mesh.vertex_binding_set;

            ASSERT(mesh_component.mesh_subsets.size() > 0);

            for (uint32_t subset = 0; subset < mesh_component.mesh_subsets.size(); ++subset) {
                AABB aabb = mesh_component.aabbs[subset];
                aabb.transform(transform->world_transform);
                if (!frustum.intersect(aabb))
                    continue;

                const MeshComponent::MeshSubset &mesh_subset = mesh_component.mesh_subsets[subset];
                draw_data.transform_index = i;
                draw_data.material_index = mesh_subset.material_index;
                draw_data.vertex_offset = mesh_subset.vertex_buffer.offset;
                draw_data.index_offset = mesh_subset.index_buffer.offset;
                draw_data.index_count = mesh_subset.index_buffer.count;

                const Material &material = materials[mesh_subset.material_index];
                if (material.is_transparent()) {
                    transparent_batches.push_back(draw_data);
                } else {
                    opaque_batches.push_back(draw_data);
                }
            }
        }

        std::sort(opaque_batches.begin(), opaque_batches.end(), [](const DrawData &lhs, const DrawData &rhs) { return lhs.vertex_buffer < rhs.vertex_buffer; });
    }

    void Scene::update_visibility_state() {
    }

    void Scene::remove_entity(Entity entity) {
        auto found = std::find(entities.begin(), entities.end(), entity);
        if (found == entities.end()) {
            Log::Warn("Entity doesn't belong to the scene");
            return;
        }

        remove_entity_tree(entity);
        entities.erase(found);
    }

    void Scene::release_all_entities() {
        for (auto entity : entities)
            remove_entity_tree(entity);
        entities.clear();
    }

    Scene::~Scene() {
        release_all_entities();

        for (auto &gpu_mesh : gpu_meshes) {
            RenderingDevice::get()->destroy_buffers(&gpu_mesh.vertex_buffer, 1);
            RenderingDevice::get()->destroy_buffers(&gpu_mesh.index_buffer, 1);
        }

        for (auto &comp_array : component_manager->component_array) {
            if (comp_array)
                ASSERT(comp_array->size() == 0);
        }
        ecs::destroy(component_manager.get());

        BufferID buffers[] = {per_frame_data_buffer, transform_buffer, material_buffer};
        RenderingDevice::get()->destroy_buffers(buffers, static_cast<uint32_t>(std::size(buffers)));
    }

} // namespace mirai