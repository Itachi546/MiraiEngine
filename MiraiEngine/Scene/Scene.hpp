#pragma once

#include "ECS.hpp"
#include "Component.hpp"
#include "Graphics/RenderingDevice.hpp"
#include <string>

namespace mirai
{
    struct ComponentManager;
    class CommandBuffer;
    class Camera;

    struct DrawData
    {
        uint32_t transform_index;
        uint32_t material_index;
        BufferID vertex_buffer;
        BufferID index_buffer;
        uint32_t vertex_offset;
        uint32_t index_offset;
        uint32_t vertex_count;
        uint32_t index_count;
    };

    class Scene
    {
      public:
        Scene(const std::string &name);

        Camera *get_camera()
        {
            return camera.get();
        }

        void add_entity(Entity entity) { entities.push_back(entity); }

        void update();

        std::string get_name() const
        {
            return name;
        }

        void set_name(const std::string &name)
        {
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

        struct FrameData
        {
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

      protected:
        std::string name;

        uint32_t K_MAX_ENTITIES = 1'000;
        bool dirty = true;

        std::unique_ptr<Camera> camera;
        glm::mat4 *transform_array;
        FrameData *frame_data_ptr;

        void remove_entity_tree(Entity entity);
        void update_transform_components();
        void update_draw_data();
    };
} // namespace mirai
