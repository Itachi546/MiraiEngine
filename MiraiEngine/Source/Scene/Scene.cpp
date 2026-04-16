#include "Scene.hpp"
#include "Component.hpp"
#include "Camera.hpp"
#include "EnvironmentMap.hpp"
#include "Animation.hpp"

#include "Device/Window.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Math/Math.hpp"
#include <execution>

namespace mirai {

    Entity Scene::create_directional_light(const std::string &name, glm::fquat orientation) {
        Entity entity = create_entity(name, entities[0]);
        TransformComponent *light_transform = ecs->component_manager->get_component<TransformComponent>(entity);
        light_transform->rotation = orientation;
        ecs->component_manager->add_component<LightComponent>(entity, LightComponent{
                                                                          .light_type = LIGHT_TYPE_DIRECTIONAL,
                                                                          .color = glm::vec3(1.0f),
                                                                          .intensity = 5.0f,
                                                                          .cast_shadow = true,
                                                                      });
        return entity;
    }

    Scene::Scene(const std::string &name) : dirty(true), name(name) {
        ecs = std::make_unique<ECS>();
        ecs->component_manager->register_component<NameComponent>();
        ecs->component_manager->register_component<HierarchyComponent>();
        ecs->component_manager->register_component<MeshComponent>();
        ecs->component_manager->register_component<TransformComponent>();
        ecs->component_manager->register_component<NodeAnimatorComponent>();
        ecs->component_manager->register_component<AnimatorComponent>();
        ecs->component_manager->register_component<LightComponent>();

        // Initialize camera/sun
        camera = std::make_unique<Camera>();

        Entity root_entity = ecs->create_entity();
        ecs->component_manager->add_component<NameComponent>(root_entity, "root");
        ecs->component_manager->add_component<TransformComponent>(root_entity);

        HierarchyComponent hierarchy_comp = {
            .parent = K_INVALID_ENTITY,
            .childrens = {},
        };
        ecs->component_manager->add_component<HierarchyComponent>(root_entity, hierarchy_comp);
        entities.push_back(root_entity);

        directional_light = create_directional_light("Sun", glm::quat(glm::radians(glm::vec3(89.0f, -24.0f, 26.0f))));

        per_frame_data.irradiance_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.prefilter_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.brdf_texture_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.padding[0] = per_frame_data.padding[1] = per_frame_data.padding[2] = 0;
    }

    void Scene::update() {
        ScopedCpuProfiling("Scene Update");

        updated_transforms.clear();
        updated_materials.clear();

        camera->update();

        if (!pause_animation) {
            update_node_animator_components();
            update_animator_components();
        }

        update_transform_components();

        update_hierarchy_components();

        update_materials();

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

        update_light_data(directional_light);

        per_frame_data.width = AppSettings::default_window_width * AppSettings::resolution_scale;
        per_frame_data.height = AppSettings::default_window_height * AppSettings::resolution_scale;
        if (env_map) {
            per_frame_data.irradiance_map = env_map->get_irradiance_map().id;
            per_frame_data.prefilter_map = env_map->get_prefilter_map().id;
            per_frame_data.brdf_texture_map = env_map->get_brdf_texture().id;
        }

        // if frustum is freezed then we don't regenerate render object list
        generate_render_object_list();

        if (updated_transforms.size() > 0)
            std::sort(updated_transforms.begin(), updated_transforms.end());
        if (updated_materials.size() > 0)
            std::sort(updated_materials.begin(), updated_materials.end());
    }

    void Scene::remove_entity_tree(Entity entity) {
        if (ecs->component_manager->has_component<HierarchyComponent>(entity)) {
            HierarchyComponent *comp = ecs->component_manager->get_component<HierarchyComponent>(entity);
            for (auto child : comp->childrens)
                remove_entity_tree(child);
        }
        ecs->destroy_entity(entity);
    }

    void Scene::update_light_data(Entity light) {
        TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(light);
        LightComponent *light_comp = ecs->component_manager->get_component<LightComponent>(light);

        // Update directional light
        per_frame_data.light_direction = quat_to_direction(transform->rotation);
        per_frame_data.cast_shadow = cast_float(light_comp->cast_shadow);
        per_frame_data.light_color = light_comp->color;
        per_frame_data.light_intensity = light_comp->intensity;
    }

    void Scene::update_materials() {
        for (uint32_t i = 0; i < materials.size(); ++i) {
            if (!materials[i]->is_dirty())
                continue;
            updated_materials.push_back(i);
            materials[i]->set_dirty(false);
        }
    }

    void Scene::update_node_animator_components() {
        auto component_array = ecs->component_manager->get_component_array<NodeAnimatorComponent>();
        std::vector<NodeAnimatorComponent> &animations = component_array->components;
        if (animations.size() == 0)
            return;
        float dt = Engine::get()->get_dt_seconds();

        for (uint32_t i = 0; i < animations.size(); ++i) {
            NodeAnimatorComponent &component = animations[i];

            if (component.current_animation_clip == K_INVALID_ANIMATION_CLIP)
                continue;
            const AnimationClip &clip = animation_clips[component.current_animation_clip];

            float duration = clip.get_duration();
            float start_time = clip.start_time;
            float end_time = clip.end_time;
            component.current_time += dt;
            if (component.looping && component.current_time > end_time)
                component.current_time = fmod(component.current_time - start_time, duration) + start_time;

            Entity entity = component_array->entities[i];
            TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);
            // This doesn't handle existing local translation/rotation/scale, need to find a way to handle that as well
            clip.sample_TRS(0, component.current_time, transform->position, transform->rotation, transform->scale);
            transform->dirty = true;
        }
    }

    void Scene::update_animator_components() {
        //@TODO Should update the animation only if the object is visible
        auto animator_component_ptr = ecs->component_manager->get_component_array<AnimatorComponent>();
        std::vector<AnimatorComponent> &animator_components = animator_component_ptr->components;

        if (animation_clips.size() == 0 || animator_components.size() == 0)
            return;

        float dt = Engine::get()->get_dt_seconds();
        for (auto &component : animator_components) {
            int current_animation_clip = component.current_animation_clip;
            if (current_animation_clip == K_INVALID_ANIMATION_CLIP)
                continue;

            Skeleton &skeleton = skeletons[component.skeleton_index];
            Pose &pose = skeleton.current_pose;

            uint32_t bone_count = cast_u32(skeleton.names.size());
            const AnimationClip &clip = animation_clips[current_animation_clip];

            float duration = clip.get_duration();
            float start_time = clip.start_time;
            float end_time = clip.end_time;

            component.current_time += dt;
            if (component.current_time > end_time)
                component.current_time = fmod(component.current_time - start_time, duration) + start_time;

            for (uint32_t i = 0; i < bone_count; ++i) {
                int parent = skeleton.parents[i];
                ASSERT(parent < int(i));

                glm::mat4 transform = parent == -1 ? glm::mat4(1.0f) : pose.matrix_palletes[parent];
                // Some of the node in hierarchy doesn't have keyframes, for such we just
                // apply parent transform with local transform
                if (clip.has_animation(i)) {
                    clip.sample_TRS(i, component.current_time, pose.joints_position[i], pose.joints_rotation[i], pose.joints_scaling[i]);
                    transform = transform * pose.get_transform(i);
                } else {
                    pose.joints_position[i] = glm::vec3(0.0f);
                    pose.joints_rotation[i] = glm::fquat(1.0f, 0.0f, 0.0f, 0.0f);
                    pose.joints_scaling[i] = glm::vec3(1.0f);
                }
                // parent_transform * animation_transform * skeleton.inv_bind_transforms[i]
                pose.matrix_palletes[i] = transform;
            }

            for (uint32_t i = 0; i < skeleton.parents.size(); ++i) {
                pose.matrix_palletes[i] = pose.matrix_palletes[i] * skeleton.inv_bind_transforms[i];
            }
        }
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
            uint32_t transform_component_index = ecs->component_manager->get_component_index<TransformComponent>(entity);
            // Update list of transforms to be patched
            updated_transforms.push_back(transform_component_index);
            force_update = true;
        }

        HierarchyComponent *hierarchy_component = ecs->component_manager->get_component<HierarchyComponent>(entity);
        if (hierarchy_component != nullptr) {
            for (auto &child : hierarchy_component->childrens)
                update_hierarchy(child, transform->world_transform, force_update);
        }
    }

    void Scene::update_hierarchy_components() {
        update_hierarchy(entities[0], glm::mat4(1.0f), false);
    }

    void Scene::generate_render_object_list() {

        // Calculated only when object is added or removed
        // @TODO calculate it only once if possible
        ScopedCpuProfiling("Update Draw Data");
        // Even though the draw data hasn't changed, we must calculate the
        // transformed AABB every frame
        if (!dirty) {

#ifdef NDEBUG
            std::for_each(std::execution::par_unseq,
                          render_object_list.begin(),
                          render_object_list.end(),
                          [&](RenderableObjectData &object) {
                              TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(object.entity);
                              object.transformed_aabb = object.local_aabb;
                              object.transformed_aabb.transform(transform->world_transform);
                          });
#else
            for (auto &object : render_object_list) {
                TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(object.entity);
                object.transformed_aabb = object.local_aabb;
                object.transformed_aabb.transform(transform->world_transform);
            }
#endif
            return;
        }

        auto mesh_component_ptr = ecs->component_manager->get_component_array<MeshComponent>();
        uint32_t component_count = static_cast<uint32_t>(mesh_component_ptr->size());

        render_object_list.clear();
        // Initially reserve some space
        render_object_list.reserve(1000);

        for (uint32_t i = 0; i < component_count; ++i) {
            MeshComponent &mesh_component = mesh_component_ptr->components[i];
            const Entity entity = mesh_component_ptr->entities[i];

            TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);

            BufferView vertex_buffer = mesh_component.vertex_buffer;
            BufferView index_buffer = mesh_component.index_buffer;
            for (uint32_t s = 0; s < mesh_component.mesh_subsets.size(); ++s) {
                MeshComponent::MeshSubset &subset = mesh_component.mesh_subsets[s];
                AABB aabb = mesh_component.aabbs[s];
                AABB transformed_aabb = aabb;
                transformed_aabb.transform(transform->world_transform);

                RenderableObjectData render_data = {
                    .entity = entity,
                    .material_index = subset.material_index,
                    .mesh_type = mesh_component.mesh_type,
                    .vertex_buffer = vertex_buffer.buffer,
                    .index_buffer = index_buffer.buffer,
                    .vertex_offset_bytes = subset.vertex_offset_bytes, // Manually calculating in shader
                    .first_index = cast_u32(subset.index_offset_bytes / sizeof(uint32_t)),
                    .index_count = subset.index_count,
                    .vertex_stride = subset.vertex_stride,
                    .local_aabb = std::move(aabb),
                    .transformed_aabb = std::move(aabb),
                };
                render_object_list.push_back(std::move(render_data));
            }

            // Sort by the buffer
            std::sort(render_object_list.begin(), render_object_list.end(), [](const RenderableObjectData &lhs, const RenderableObjectData &rhs) {
                return lhs.vertex_buffer < rhs.vertex_buffer;
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