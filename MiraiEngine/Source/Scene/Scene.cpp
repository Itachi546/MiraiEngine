#include "Scene.hpp"
#include "Component.hpp"
#include "Camera.hpp"
#include "EnvironmentMap.hpp"

#include "Device/Window.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"
#include "Math/MathUtils.hpp"

#include <execution>
#include <algorithm>

namespace mirai {

    Scene::Scene(const std::string &name) : name(name), dirty(true) {
        ecs = std::make_unique<ECS>();
        ecs->component_manager->register_component<NameComponent>();
        ecs->component_manager->register_component<HierarchyComponent>();
        ecs->component_manager->register_component<MeshComponent>();
        ecs->component_manager->register_component<TransformComponent>();

        RenderingDevice *device = RenderingDevice::get();
        directional_light_info.enable_shadow = true;

        // Initialize camera/sun
        camera = std::make_unique<Camera>();
        sun = std::make_unique<Light>();
        sun->color = glm::vec3(1.0f);
        sun->rotation = glm::vec3(0.0f, 82.0f, 0.0f);
        sun->intensity = 20.0f;
        sun->cast_shadow = true;

        per_frame_data.irradiance_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.prefilter_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.brdf_texture_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.padding[0] = per_frame_data.padding[1] = per_frame_data.padding[2] = 0;
    }

    void Scene::on_initialize() {
        // @TODO This mustn't be done once we bake the transform matrix into position, rotation, scale
        // and apply it at the beginning of everything
        // Maybe we should update hierarchy when we create mesh and initialize it
        update_transform_components();

        update_hierarchy_component();

        generate_render_object_list();
    }

    void Scene::update() {
        ScopedCpuProfiling("Scene Update");

        camera->update();

        update_transform_components();

        update_hierarchy_component();

        uint32_t width, height;
        Window::get()->get_size(&width, &height);

        camera->set_aspect_ratio(float(width) / float(height));
        glm::mat4 P = camera->get_projection_transform();
        glm::mat4 V = camera->get_view_transform();
        glm::mat4 VP = camera->get_view_projection_transform();

        per_frame_data.P = P;
        per_frame_data.V = V;
        per_frame_data.VP = VP;
        per_frame_data.invVP = camera->get_inv_view_projection_transform();
        per_frame_data.camera_position = camera->position;
        per_frame_data.elapsed_time = Engine::get()->get_elapsed_seconds();

        per_frame_data.light_direction = sun->get_direction();
        per_frame_data.cast_shadow = cast_float(sun->cast_shadow);
        per_frame_data.light_color = sun->color;
        per_frame_data.light_intensity = sun->intensity;

        per_frame_data.width = cast_float(width);
        per_frame_data.height = cast_float(height);
        if (env_map) {
            per_frame_data.irradiance_map = env_map->get_irradiance_map().id;
            per_frame_data.prefilter_map = env_map->get_prefilter_map().id;
            per_frame_data.brdf_texture_map = env_map->get_brdf_texture().id;
        }

        generate_render_object_list();

        Frustum &frustum = camera->get_frustum();

        main_render_batches.clear();
        DrawBatchGenerator::CreateBatch(this, &frustum, main_render_batches, false);

        glm::vec3 light_direction = sun->get_direction();
    }

    void Scene::remove_entity_tree(Entity entity) {
        if (ecs->component_manager->has_component<HierarchyComponent>(entity)) {
            HierarchyComponent *comp = ecs->component_manager->get_component<HierarchyComponent>(entity);
            for (auto child : comp->childrens)
                remove_entity_tree(child);
        }
        ecs->destroy_entity(entity);
    }

    void Scene::update_transform_components() {
        auto transform_array_ptr = ecs->component_manager->get_component_array<TransformComponent>();
        std::vector<TransformComponent> &transforms = transform_array_ptr->components;
        std::for_each(std::execution::par_unseq,
                      transforms.begin(),
                      transforms.end(),
                      [](TransformComponent &transform) { transform.update_local_transform(); });
    }

    void Scene::update_hierarchy(Entity entity, const glm::mat4 &parent_transform, bool force_update) {
        TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);
        if (transform->dirty || force_update) {
            transform->world_transform = parent_transform * transform->local_transform;
            transform->dirty = false;

            HierarchyComponent *hierarchy_component = ecs->component_manager->get_component<HierarchyComponent>(entity);
            if (hierarchy_component != nullptr) {
                for (auto &child : hierarchy_component->childrens)
                    update_hierarchy(child, transform->world_transform, true);
            }
        }
    }

    void Scene::update_hierarchy_component() {
        for (auto &entity : entities)
            update_hierarchy(entity, glm::mat4(1.0f), false);

        std::vector<TransformComponent> &transforms = ecs->component_manager->get_component_array<TransformComponent>()->components;
    }

    void Scene::generate_render_object_list() {

        // Calculated only when object is added or removed
        // @TODO calculate it only once if possible
        if (!dirty)
            return;

        ScopedCpuProfiling("Update Draw Data");

        auto mesh_component_ptr = ecs->component_manager->get_component_array<MeshComponent>();
        std::vector<Entity> &entities = mesh_component_ptr->entities;
        uint32_t component_count = static_cast<uint32_t>(mesh_component_ptr->size());

        render_object_list.clear();
        // Initially reserve some space
        render_object_list.reserve(1000);

        for (uint32_t i = 0; i < component_count; ++i) {
            MeshComponent &mesh_component = mesh_component_ptr->components[i];
            const Entity entity = mesh_component_ptr->entities[i];

            GpuMesh &gpu_mesh = gpu_meshes[mesh_component.gpu_mesh_index];
            TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);

            BufferView vertex_buffer = mesh_component.vertex_buffer;
            BufferView index_buffer = mesh_component.index_buffer;
            for (uint32_t s = 0; s < mesh_component.mesh_subsets.size(); ++s) {
                MeshComponent::MeshSubset &subset = mesh_component.mesh_subsets[s];
                AABB aabb = mesh_component.aabbs[s];
                RenderableObjectData render_data = {
                    .entity = entity,
                    .material_index = subset.material_index,
                    .render_flags = mesh_component._flags,
                    .vertex_buffer = vertex_buffer,
                    .index_buffer = index_buffer,
                    .vertex_offset = subset.vertex_offset / sizeof(Vertex),
                    .vertex_count = subset.vertex_count,
                    .index_offset = subset.index_offset / sizeof(uint32_t),
                    .index_count = subset.index_size / sizeof(uint32_t),
                    .aabb = std::move(aabb),
                    .vertex_binding_set = gpu_mesh.vertex_binding_set,
                };

                render_object_list.push_back(std::move(render_data));
            }

            // Sort by the buffer
            std::sort(render_object_list.begin(), render_object_list.end(), [](const RenderableObjectData &lhs, const RenderableObjectData &rhs) {
                return lhs.vertex_buffer.buffer.id < rhs.vertex_buffer.buffer.id;
            });
        }

        dirty = false;
    }

    void Scene::remove_entity(Entity entity) {
        auto found = std::find(entities.begin(), entities.end(), entity);
        if (found == entities.end()) {
            Log::Warn("Entity doesn't belong to the scene");
            return;
        }

        dirty = true;
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

        for (auto &comp_array : ecs->component_manager->component_array) {
            if (comp_array)
                ASSERT(comp_array->size() == 0);
        }
        ecs->destroy();
        ecs = nullptr;
    }

} // namespace mirai