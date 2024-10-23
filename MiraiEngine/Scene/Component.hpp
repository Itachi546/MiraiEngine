#pragma once

#include "ECS.hpp"
#include "Graphics/RenderingDevice.hpp"
#include <glm/glm.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace mirai
{
    struct NameComponent
    {
        std::string name;
    };

    struct HierarchyComponent
    {
        std::vector<Entity> childrens;
        Entity parent;

        void set_parent(Entity parent)
        {
            this->parent = parent;
        }

        void add_children(Entity children)
        {
            auto found = std::find(childrens.begin(), childrens.end(), children);
            if (found != childrens.end())
                childrens.push_back(children);
            else
                Log::Warn("Trying to add duplicate children");
        }

        void remove_children(Entity children)
        {
            auto found = std::find(childrens.begin(), childrens.end(), children);
            if (found != childrens.end())
            {
                childrens.erase(found);
            }
        }

        void clear_parent()
        {
            this->parent = K_INVALID_ENTITY;
        }
    };
    struct BufferView
    {
        uint32_t offset;
        uint32_t size;
    };

    struct Vertex
    {
        glm::vec3 position;
        uint32_t normal;

        uint32_t tangent;
        uint32_t bitangent;
        glm::vec2 uv;
    };

    static_assert(sizeof(Vertex) % 16 == 0);

    struct MeshDataComponent
    {
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        BufferID vertex_buffer;
        BufferID index_buffer;

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;

        void prepare_render_data()
        {
            // @TODO upload to buffer
            vertex_buffer_size = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
            index_buffer_size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));
        }
    };

    struct MeshComponent
    {
        enum FLAGS
        {
            EMPTY = 0,
            RENDERABLE = 1 << 0,
            DYNAMIC = 1 << 2,
            CAST_SHADOW = 1 << 2,
            RECEIVE_SHADOW = 1 << 3,
            DEPTH_TEST = 1 << 4,
            DEPTH_WRITE = 1 << 5
        };

        uint32_t _flags = RENDERABLE | DEPTH_TEST | DEPTH_WRITE | CAST_SHADOW | RECEIVE_SHADOW;

        uint32_t mesh_data_comp_index;
        struct MeshSubset
        {
            BufferView vertex_buffer;
            BufferView index_buffer;
            uint32_t vertex_count;
        };
        std::vector<MeshSubset> mesh_subsets;
    };

    struct TransformComponent
    {
        TransformComponent() : position(glm::vec3(0.0f)),
                               rotation(glm::fquat()),
                               local_transform(glm::mat4(1.0f)),
                               world_transform(glm::mat4(1.0f)),
                               dirty(true)
        {
        }

        glm::vec3 position;
        bool dirty;

        glm::fquat rotation;
        glm::vec3 scale;

        glm::mat4 local_transform;
        glm::mat4 world_transform;
    };

    struct MaterialComponent
    {
        std::string name;

        glm::vec4 albedo;
        glm::vec4 emission;

        float metallic_factor;
        float roughness_factor;
        float transmission;
        bool receive_shadow;
        bool cast_shadow;

        TextureID albedo_texture;
        TextureID normal_texture;
        TextureID metallic_roughness_texture;
        TextureID occlusion_texture;
    };
}; // namespace mirai