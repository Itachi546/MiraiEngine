#pragma once

#include "ECS.hpp"
#include "Component.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "RenderBatch.hpp"
#include "Material.hpp"

#include <string>
#include <mutex>

namespace mirai {
    struct ComponentManager;
    class CommandBuffer;
    class Camera;
    class EnvironmentMap;

    constexpr const uint32_t NUM_DIRLIGHT_CASCADE = 4;
    struct DirectionalLightCascadeInfo {
        glm::mat4 VP[NUM_DIRLIGHT_CASCADE];
        float split_distances[4];

        float z_range;
        float width;
        float height;
        float _padding;
    };

    struct DirectionalLightInfo {
        bool enable_shadow;
        DirectionalLightCascadeInfo cascade_info;
        uint32_t cascade_set_binding_id;
    };
    struct RenderableObjectData {
        Entity entity;
        uint32_t material_index;
        uint32_t render_flags;

        BufferView vertex_buffer;
        BufferView index_buffer;

        uint32_t vertex_offset;
        uint32_t vertex_count;

        uint32_t index_offset;
        uint32_t index_count;

        AABB aabb;

        UniformSetID vertex_binding_set;
    };

    struct GpuMesh {
        BufferView vertex_buffer;
        BufferView index_buffer;

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        UniformSetID vertex_binding_set;
    };

    class Scene {
      public:
        Scene(const std::string &name);

        // Called after everything is initialized
        void on_initialize();

        Camera *get_camera() {
            return camera.get();
        }

        EnvironmentMap *get_environment_map() {
            return env_map.get();
        }

        Light *get_sun() {
            return sun.get();
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

        void remove_entity(Entity entity);

        void release_all_entities();

        void generate_render_object_list();

        virtual ~Scene();

        std::unique_ptr<ECS> ecs;
        std::vector<std::unique_ptr<Material>> materials;
        std::vector<Entity> entities;

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

        DirectionalLightInfo directional_light_info;

        std::vector<GpuMesh> gpu_meshes;
        std::vector<RenderableObjectData> render_object_list;
        std::vector<RenderBatch> main_render_batches;
        bool dirty;

      protected:
        std::string name;

        std::unique_ptr<Camera> camera;
        std::unique_ptr<Light> sun;
        std::shared_ptr<EnvironmentMap> env_map;

        std::mutex mu;

        void remove_entity_tree(Entity entity);
        void update_transform_components();
        void update_hierarchy_component();
        void update_hierarchy(Entity entity, const glm::mat4 &parent_transform, bool force_update = false);
    };
} // namespace mirai
