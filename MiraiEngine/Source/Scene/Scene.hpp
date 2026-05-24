#pragma once

#include "ECS.hpp"
#include "Component.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Material.hpp"
#include "Animation.hpp"
#include <string>

namespace mirai {
    struct ComponentManager;
    class CommandBuffer;
    class Camera;
    class EnvironmentMap;

    struct Renderable {
        Entity entity;
        uint32_t transform_index;
        MeshType mesh_type;
        uint32_t mesh_index;

        uint32_t material_index;
        uint32_t mesh_flags;
        AABB transformed_aabb;
    };

    class Scene {
      public:
        Scene(const std::string &name);

        Camera *get_camera() {
            return camera.get();
        }

        EnvironmentMap *get_environment_map() {
            return env_map.get();
        }

        Entity get_default_directional_light() {
            return directional_light;
        }

        void add_entity(Entity entity) { entities.push_back(entity); }

        void update();

        std::string get_name() const {
            return name;
        }

        void set_name(const std::string &name) {
            this->name = name;
        }

        void set_environment_map(std::shared_ptr<EnvironmentMap> env_map) {
            this->env_map = env_map;
        }

        Entity create_entity(const std::string &name, Entity parent_entity = K_INVALID_ENTITY) {
            Entity entity = ecs->create_entity();
            ecs->component_manager->add_component<TransformComponent>(entity);
            ecs->component_manager->add_component<NameComponent>(entity, NameComponent{name});

            // We choose the scene root node as parent entity if the parent is invalid
            parent_entity = parent_entity == K_INVALID_ENTITY ? entities[0] : parent_entity;

            // This order should be strictly maintained as, it invalidates the vector after insertion
            HierarchyComponent &current = ecs->component_manager->add_component<HierarchyComponent>(entity);
            HierarchyComponent *parent = ecs->component_manager->get_component<HierarchyComponent>(parent_entity);
            current.set_parent(entities[0]);
            parent->add_children(entity);

            entities.push_back(entity);
            return entity;
        }

        Entity create_entity(const std::string &name, uint32_t mesh_index, uint32_t material_index, Entity parent_entity);
        Entity create_plane(const std::string &name, Entity parent_entity = K_INVALID_ENTITY);
        Entity create_cube(const std::string &name, Entity parent_entity = K_INVALID_ENTITY);
        Entity create_sphere(const std::string &name, Entity parent_entity = K_INVALID_ENTITY);

        void remove_entity(Entity entity);

        void release_all_entities();

        void generate_render_object_list();

        virtual ~Scene();

        std::unique_ptr<ECS> ecs;
        std::vector<Entity> entities;

        std::vector<std::unique_ptr<Material3D>> materials;
        std::vector<MeshAllocation> mesh_allocations;

        std::vector<Renderable> render_object_list;

        std::vector<std::unique_ptr<SkeletalAsset>> skeletal_assets;
        std::vector<std::unique_ptr<AnimationPlayer>> animation_players;

        // List of material/transforms that must be patched on gpu
        // first represent the transform index and second represent the index in GPU
        std::vector<std::pair<uint32_t, uint32_t>> updated_transforms;
        std::vector<uint32_t> updated_materials;
        std::vector<uint32_t> updated_lights;

        struct FrameData {
            glm::mat4 P;
            glm::mat4 V;
            glm::mat4 VP;
            glm::mat4 invVP;
            glm::mat4 prev_VP;

            glm::vec2 current_frame_jitter;
            glm::vec2 prev_frame_jitter;

            glm::vec3 camera_position;
            float elapsed_time;

            float width;
            float height;
            uint32_t irradiance_map;
            uint32_t prefilter_map;

            uint32_t brdf_texture_map;
            uint32_t padding[3];
        } per_frame_data;
        static_assert(sizeof(FrameData) % 16 == 0);

        bool pause_animation = false;
        float animation_speed = 1.0f;
        int jitter_index = 0;
        int jitter_period = 4;

      protected:
        std::string name;
        Entity directional_light;
        std::unique_ptr<Camera> camera;
        std::shared_ptr<EnvironmentMap> env_map;

        enum DefaultMeshType {
            Plane = 0,
            Count
        };

        uint32_t plane_mesh_index = K_INVALID_ID;
        uint32_t cube_mesh_index = K_INVALID_ID;
        uint32_t sphere_mesh_index = K_INVALID_ID;

        std::mutex mu;

        void remove_entity_tree(Entity entity);
        void update_node_animator_components();
        void update_animation_players();
        void update_hierarchy_components();
        void update_hierarchy(Entity entity, const glm::mat4 &parent_transform, bool force_update = false);

        Entity create_directional_light(const std::string &name, glm::fquat orientation);

        friend class Renderer;
    };
} // namespace mirai
