#pragma once

#include "ECS.hpp"
#include "Component.hpp"
#include "Graphics/RenderingDevice.hpp"
#include <string>

namespace mirai {
    struct ComponentManager;
    class CommandBuffer;
    class Camera;

    struct DrawData {
        uint32_t transform_index;
        uint32_t material_index;

        BufferID vertex_buffer;
        BufferID index_buffer;

        uint32_t vertex_offset;
        uint32_t index_offset;

        uint32_t index_count;
        UniformSetID vertex_binding_set;
    };

    struct GpuMesh {
        BufferID vertex_buffer;
        BufferID index_buffer;

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        UniformSetID vertex_binding_set;
    };

    class Scene {
      public:
        Scene(const std::string &name);

        Camera *get_camera() {
            return camera.get();
        }

        void add_entity(Entity entity) { entities.push_back(entity); }

        void update();

        std::string get_name() const {
            return name;
        }

        void set_name(const std::string &name) {
            this->name = name;
        }

        void remove_entity(Entity entity);

        void release_all_entities();

        virtual ~Scene();

        std::unique_ptr<ComponentManager> component_manager;
        std::vector<Material> materials;
        std::vector<Entity> entities;
        std::vector<DrawData> draw_infos;

        BufferID transform_buffer;
        glm::mat4 *transform_array;

        struct FrameData {
            glm::mat4 P;
            glm::mat4 V;
            glm::mat4 VP;

            glm::vec3 camera_position;
            float elapsed_time;

            glm::vec2 window_size;
            glm::vec2 _padding;
        };
        static_assert(sizeof(FrameData) % 16 == 0);

        BufferID per_frame_data_buffer;
        UniformSetID per_frame_uniform_set;

        std::vector<GpuMesh> gpu_meshes;

      protected:
        std::string name;

        bool dirty = true;

        std::unique_ptr<Camera> camera;
        FrameData *frame_data_ptr;

        void remove_entity_tree(Entity entity);
        void update_transform_components();
        void update_hierarchy_component();
        void update_hierarchy(Entity entity, const glm::mat4 &parent_transform);
        void update_draw_data();
    };
} // namespace mirai
