#include "Scene.hpp"
#include "ShaderMaterial.hpp"
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

        // Per frame staging buffer
        buffer_desc.size = cast_u32(utils::mb_to_bytes(16));
        buffer_desc.usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT;
        per_frame_staging_buffer = device->create_buffer(&buffer_desc, "per_frame_staging_buffer");
        per_frame_staging_buffer_ptr = device->map_buffer(per_frame_staging_buffer);

        // Initialize PerFrame Resources
        buffer_desc = {
            .size = sizeof(FrameData),
            .usage_flags = BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
        };
        per_frame_uniform_buffer = device->create_buffer(&buffer_desc, "per_frame_data_buffer");

        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX,
        };
        per_frame_uniform_set = device->create_uniform_set(&layout, 1, 0, "per_frame_uniform_set");

        UniformBinding binding = {
            .resource_id = per_frame_uniform_buffer,
            .buffer_info = {
                .offset = 0,
                .range = sizeof(FrameData),
            },
        };
        device->update_uniform_set(per_frame_uniform_set, &binding, 1);

        // Initialize cascade info
        buffer_desc.size = sizeof(DirectionalLightCascadeInfo);
        cascade_uniform_buffer = device->create_buffer(&buffer_desc, "cascade_uniform_buffer");

        directional_light_info.enable_shadow = true;
        if (directional_light_info.enable_shadow) {

            UniformLayout cascade_buffer_layout = {
                .binding = 0,
                .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
                .shader_stage = SHADER_STAGE_VERTEX,
            };
            directional_light_info.cascade_uniform_set = device->create_uniform_set(&cascade_buffer_layout, 1, 0, "cascade_uniform_set");

            uint32_t set_id = 1;
            directional_light_info.cascade_set_binding_id = set_id;
            UniformBinding cascade_uniform_binding = {.resource_id = cascade_uniform_buffer};
            device->update_uniform_set(directional_light_info.cascade_uniform_set, &cascade_uniform_binding, set_id);
        }

        // Initialize camera/sun
        camera = std::make_unique<Camera>();
        sun = std::make_unique<Light>();
        sun->color = glm::vec3(1.0f);
        sun->rotation = glm::vec3(0.0f, 82.0f, 0.0f);
        sun->intensity = 20.0f;
        sun->cast_shadow = true;
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

        per_frame_data.elapsed_time = Engine::get()->get_elapsed_seconds();
        per_frame_data.P = P;
        per_frame_data.V = V;
        per_frame_data.VP = VP;
        per_frame_data.window_size = glm::vec2((float)width, (float)height);

        generate_render_object_list();

        Frustum &frustum = camera->get_frustum();

        main_render_batches.clear();
        DrawBatchGenerator::CreateBatch(this, &frustum, main_render_batches, false);
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

        std::vector<TransformComponent> &transforms = component_manager->get_component_array<TransformComponent>()->components;
        for (uint32_t i = 0; i < transforms.size(); ++i)
            transform_array[i] = transforms[i].world_transform;
    }

    void Scene::generate_render_object_list() {

        // Calculated only when object is added or removed
        // @TODO calculate it only once if possible
        if (!dirty)
            return;

        memcpy(material_array, materials.data(), sizeof(Material) * materials.size());
        ScopedCpuProfiling("Update Draw Data");

        auto mesh_component_ptr = component_manager->get_component_array<MeshComponent>();
        std::vector<Entity> &entities = mesh_component_ptr->entities;
        uint32_t component_count = static_cast<uint32_t>(mesh_component_ptr->size());

        render_object_list.clear();
        // Initially reserve some space
        render_object_list.reserve(1000);

        for (uint32_t i = 0; i < component_count; ++i) {
            MeshComponent &mesh_component = mesh_component_ptr->components[i];
            const Entity entity = mesh_component_ptr->entities[i];

            GpuMesh &gpu_mesh = gpu_meshes[mesh_component.gpu_mesh_index];
            TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
            uint32_t transform_index = component_manager->get_component_index<TransformComponent>(entity);
            for (uint32_t s = 0; s < mesh_component.mesh_subsets.size(); ++s) {
                MeshComponent::MeshSubset &subset = mesh_component.mesh_subsets[s];

                AABB aabb = mesh_component.aabbs[s];
                aabb.transform(transform->world_transform);

                RenderableObjectData render_data = {
                    .transform_index = transform_index,
                    .material_index = subset.material_index,
                    .vertex_buffer = gpu_mesh.vertex_buffer,
                    .index_buffer = gpu_mesh.index_buffer,
                    .vertex_offset = subset.vertex_buffer.offset,
                    .vertex_count = subset.vertex_buffer.count,
                    .index_offset = subset.index_buffer.offset,
                    .index_count = subset.index_buffer.count,
                    .aabb = std::move(aabb),
                    .vertex_binding_set = gpu_mesh.vertex_binding_set,
                };

                render_object_list.push_back(std::move(render_data));
            }

            // Sort by the buffer
            std::sort(render_object_list.begin(), render_object_list.end(), [](const RenderableObjectData &lhs, const RenderableObjectData &rhs) {
                return lhs.vertex_buffer.id < rhs.vertex_buffer.id;
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

        for (auto &gpu_mesh : gpu_meshes) {
            RenderingDevice::get()->destroy_buffers(&gpu_mesh.vertex_buffer, 1);
            RenderingDevice::get()->destroy_buffers(&gpu_mesh.index_buffer, 1);
        }

        for (auto &comp_array : component_manager->component_array) {
            if (comp_array)
                ASSERT(comp_array->size() == 0);
        }
        ecs::destroy(component_manager.get());

        BufferID buffers[] = {per_frame_uniform_buffer, transform_buffer, material_buffer, cascade_uniform_buffer, per_frame_staging_buffer};
        RenderingDevice::get()->destroy_buffers(buffers, static_cast<uint32_t>(std::size(buffers)));
    }

} // namespace mirai