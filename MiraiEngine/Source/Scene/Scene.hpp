#pragma once

#include "ECS.hpp"
#include "Component.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Material.hpp"
#include "Animation.hpp"
#include <string>
#include <mutex>

namespace mirai {
    struct ComponentManager;
    class CommandBuffer;
    class Camera;
    class EnvironmentMap;

    struct RenderableObjectData {
        Entity entity;
        uint32_t material_index;
        MeshType mesh_type;

        BufferID vertex_buffer;
        BufferID index_buffer;

        uint32_t vertex_offset_bytes;
        uint32_t first_index;
        uint32_t index_count;
        uint32_t vertex_stride;

        AABB local_aabb;
        AABB transformed_aabb;
    };

    struct GpuMesh {
        BufferView vertex_buffer;
        BufferView index_buffer;

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;

        std::vector<uint8_t> vertices;
        std::vector<uint32_t> indices;
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

        Entity create_entity() {
            Entity entity = ecs->create_entity();
            entities.push_back(entity);
            return entity;
        }

        void remove_entity(Entity entity);

        void release_all_entities();

        void generate_render_object_list();

        virtual ~Scene();

        std::unique_ptr<ECS> ecs;
        std::vector<std::unique_ptr<Material3D>> materials;
        std::vector<AnimationClip> animation_clips;
        std::vector<Skeleton> skeletons;
        std::vector<Entity> entities;

        // List of material/transforms that must be patched on gpu
        std::vector<uint32_t> updated_transforms;
        std::vector<uint32_t> updated_materials;

        struct FrameData {
            glm::mat4 P;
            glm::mat4 V;
            glm::mat4 VP;
            glm::mat4 invVP;

            glm::vec3 camera_position;
            float elapsed_time;

            glm::vec3 light_direction;
            float cast_shadow;

            glm::vec3 light_color;
            float light_intensity;

            float width;
            float height;
            uint32_t irradiance_map;
            uint32_t prefilter_map;

            uint32_t brdf_texture_map;
            uint32_t padding[3];
        } per_frame_data;
        static_assert(sizeof(FrameData) % 16 == 0);

        std::vector<GpuMesh> gpu_meshes;
        std::vector<RenderableObjectData> render_object_list;
        bool dirty;

        bool pause_animation = false;

      protected:
        std::string name;
        Entity directional_light;
        std::unique_ptr<Camera> camera;
        std::shared_ptr<EnvironmentMap> env_map;

        std::mutex mu;

        void remove_entity_tree(Entity entity);
        void update_light_data(Entity light);
        void update_materials();
        void update_node_animator_components();
        void update_animator_components();
        void update_transform_components();
        void update_hierarchy_components();
        void update_hierarchy(Entity entity, const glm::mat4 &parent_transform, bool force_update = false);

        Entity create_directional_light(const std::string &name, glm::fquat orientation);
    };
} // namespace mirai
