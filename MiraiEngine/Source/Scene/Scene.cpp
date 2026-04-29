#include "Scene.hpp"
#include "Component.hpp"
#include "Camera.hpp"
#include "EnvironmentMap.hpp"
#include "Animation.hpp"
#include "Common/JobSystem.hpp"
#include "Device/Window.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Common/Random.hpp"
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

    Scene::Scene(const std::string &name) : name(name) {
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

        directional_light = create_directional_light("Sun", glm::quat(glm::radians(glm::vec3(110.0f, -71.0f, 0.0f))));

        per_frame_data.irradiance_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.prefilter_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.brdf_texture_map = K_INVALID_RESOURCE_HANDLE;
        per_frame_data.padding[0] = per_frame_data.padding[1] = per_frame_data.padding[2] = 0;
        per_frame_data.current_frame_jitter = glm::vec2(0.0f);
    }

    void Scene::update() {
        ScopedCpuProfiling("Scene Update");

        updated_transforms.clear();
        updated_materials.clear();

        // Update TAA
        if (AppSettings::enable_taa) {
            per_frame_data.prev_VP = camera->get_view_projection_transform();
            per_frame_data.prev_frame_jitter = per_frame_data.current_frame_jitter;
            uint32_t width = AppSettings::get_width();
            uint32_t height = AppSettings::get_height();
            glm::vec2 inv_resolution = 1.0f / glm::vec2{cast_float(width), cast_float(height)};

            glm::vec2 current_frame_jitter = halton23_sequence(jitter_index) * 2.0f - 1.0f;
            current_frame_jitter *= inv_resolution;
            per_frame_data.current_frame_jitter = current_frame_jitter;

            jitter_index = (jitter_index + 1) % jitter_period;
            camera->set_jitter_factor(current_frame_jitter);
        } else {
            camera->set_jitter_factor(glm::vec2(0.0f));
        }

        camera->update();

        if (!pause_animation) {
            update_node_animator_components();
            update_animation_players();
        }

        // Update changed materials
        uint32_t material_count = cast_u32(materials.size());
        std::mutex mu;
        jobsystem::Dispatch(material_count, 64, [&](jobsystem::JobDispatchArg arg) {
            if (materials[arg.job_index]->is_dirty()) {
                {
                    std::lock_guard<std::mutex> lk(mu);
                    updated_materials.push_back(arg.job_index);
                }
                materials[arg.job_index]->set_dirty(false);
            }
        });

        // Update Transforms
        auto &transform_components = ecs->component_manager->get_component_array<TransformComponent>()->components;
        uint32_t transform_count = cast_u32(transform_components.size());
        jobsystem::Dispatch(transform_count, 64, [&transform_components](jobsystem::JobDispatchArg arg) {
            transform_components[arg.job_index].update_local_transform();
        });
        jobsystem::Wait();

        update_hierarchy_components();

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

    void Scene::update_node_animator_components() {
        auto component_array = ecs->component_manager->get_component_array<NodeAnimatorComponent>();
        std::vector<NodeAnimatorComponent> &animations = component_array->components;
        if (animations.size() == 0)
            return;
        float dt = Engine::get()->get_dt_seconds();

        for (uint32_t i = 0; i < animations.size(); ++i) {
            NodeAnimatorComponent &component = animations[i];

            uint32_t current_animation_clip = component.current_animation_clip;
            if (current_animation_clip == K_INVALID_ANIMATION_CLIP)
                continue;
            ASSERT(current_animation_clip < component.animation_clips.size());
            const AnimationClip *clip = &component.animation_clips[current_animation_clip];

            float duration = clip->get_duration();
            float start_time = clip->start_time;
            float end_time = clip->end_time;
            component.current_time += dt * animation_speed;

            if (clip->looping && component.current_time > end_time)
                component.current_time = fmod(component.current_time - start_time, duration) + start_time;

            Entity entity = component_array->entities[i];
            TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);

            // This doesn't handle existing local translation/rotation/scale, need to find a way to handle that as well
            clip->sample_TRS(0, component.current_time, transform->position, transform->rotation, transform->scale);
            transform->dirty = true;
        }
    }

    void Scene::update_animation_players() {
        if (animation_players.size() == 0)
            return;

        float dt = Engine::get()->get_dt_seconds();
        for (auto &animation_player : animation_players) {
            const Skeleton *skeleton = &animation_player->skeletal_asset->skeleton;
            uint32_t bone_count = cast_u32(skeleton->names.size());
            if (!animation_player->is_valid()) {
                for (uint32_t i = 0; i < bone_count; ++i) {
                    animation_player->matrix_palletes[i] = glm::mat4(1.0f);
                }
                return;
            }

            animation_player->current_pose.resize(bone_count);

            animation_player->blend_time = std::clamp(animation_player->blend_time + dt, 0.0f, animation_player->blend_duration);
            float blend_factor = animation_player->blend_time / animation_player->blend_duration;

            animation_player->sample_current_animation(dt * animation_speed);
            animation_player->sample_target_animation(dt * animation_speed);

            glm::vec3 min = glm::vec3(FLT_MAX);
            glm::vec3 max = glm::vec3(-FLT_MAX);
            const float K_BONE_RADIUS = 1.0f;

            std::vector<glm::mat4> &matrix_palletes = animation_player->matrix_palletes;
            Pose &pose = animation_player->current_pose;

            for (uint32_t i = 0; i < bone_count; ++i) {
                int parent = skeleton->parents[i];
                ASSERT(parent < int(i));

                const glm::mat4 &parent_transform = parent == -1 ? glm::mat4(1.0f) : matrix_palletes[parent];

                animation_player->evaluate_pose_for_bone(pose, i, blend_factor);
                matrix_palletes[i] = parent_transform * pose.get_transform(i);

                // World space AABB
                glm::vec3 bone_pos = parent_transform * glm::vec4(pose.joints_position[i], 1.0f);
                glm::vec3 bone_min = bone_pos - glm::vec3(K_BONE_RADIUS);
                glm::vec3 bone_max = bone_pos + glm::vec3(K_BONE_RADIUS);
                min = glm::min(bone_min, min);
                max = glm::max(bone_max, max);
            }
            animation_player->aabb = {min, max};

            for (uint32_t i = 0; i < skeleton->parents.size(); ++i) {
                matrix_palletes[i] = matrix_palletes[i] * skeleton->inv_bind_transforms[i];
            }
        }
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

        auto mesh_component_ptr = ecs->component_manager->get_component_array<MeshComponent>();
        uint32_t component_count = static_cast<uint32_t>(mesh_component_ptr->size());
        render_object_count.store(0u);

        jobsystem::Dispatch(component_count, 64, [&](jobsystem::JobDispatchArg arg) {
            MeshComponent &mesh_component = mesh_component_ptr->components[arg.job_index];
            bool is_skinned = false;

            const Entity entity = mesh_component_ptr->entities[arg.job_index];
            const TransformComponent *transform = ecs->component_manager->get_component<TransformComponent>(entity);

            AnimatorComponent *animator = ecs->component_manager->get_component<AnimatorComponent>(entity);
            is_skinned = animator ? true : false;

            BufferView vertex_buffer = mesh_component.vertex_buffer;
            BufferView index_buffer = mesh_component.index_buffer;

            for (uint32_t s = 0; s < mesh_component.mesh_subsets.size(); ++s) {
                MeshComponent::MeshSubset &subset = mesh_component.mesh_subsets[s];
                AABB transformed_aabb = mesh_component.aabbs[s];

                // Create a combined AABB from animated pose and rest pose
                if (is_skinned) {
                    const auto &animation_player = animation_players[animator->animation_player_index];
                    if (animation_player->is_valid()) {
                        const AABB &animation_aabb = animation_player->aabb;
                        transformed_aabb.combine(animation_aabb);
                    }
                }
                transformed_aabb.transform(transform->world_transform);

                uint32_t index = render_object_count.fetch_add(1, std::memory_order_relaxed);
                ASSERT(index < K_MAX_ENTITIES);

                render_object_list[index] = RenderableObjectData{
                    .entity = entity,
                    .material_index = subset.material_index,
                    .mesh_type = mesh_component.mesh_type,
                    .vertex_buffer = vertex_buffer.buffer,
                    .index_buffer = index_buffer.buffer,
                    .vertex_offset_bytes = is_skinned ? subset.output_vertex_offset_bytes : subset.vertex_offset_bytes, // Manually calculating in shader
                    .first_index = cast_u32(subset.index_offset_bytes / sizeof(uint32_t)),
                    .index_count = subset.index_count,
                    .vertex_stride = VERTEX_DATA_SIZE,
                    .local_aabb = mesh_component.aabbs[s],
                    .transformed_aabb = transformed_aabb,
                };
            }
        });
        jobsystem::Wait();
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

        for (auto &comp_array : ecs->component_manager->component_array) {
            if (comp_array)
                ASSERT(comp_array->size() == 0);
        }
        ecs->destroy();
        ecs = nullptr;
    }

} // namespace mirai