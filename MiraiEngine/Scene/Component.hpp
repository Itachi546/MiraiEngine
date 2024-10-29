#pragma once

#include "ECS.hpp"
#include "Graphics/RenderingDevice.hpp"
#include <glm/glm.hpp>

#include <glm/gtc/quaternion.hpp>

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
            if (found == childrens.end())
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
        uint32_t count;
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

        struct MeshSubset
        {
            BufferView vertex_buffer;
            BufferView index_buffer;
            uint32_t vertex_count;
            uint32_t material_index;
        };
        std::vector<MeshSubset> mesh_subsets;

        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        BufferID vertex_buffer{K_INVALID_ID};
        BufferID index_buffer{K_INVALID_ID};

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;

        void prepare_render_data()
        {
            // @TODO upload to buffer
            vertex_buffer_size = static_cast<uint32_t>(vertices.size() * sizeof(Vertex));
            index_buffer_size = static_cast<uint32_t>(indices.size() * sizeof(uint32_t));

            BufferDescription buffer_desc = {
                .size = vertex_buffer_size,
                .usage_flags = BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT,
                .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
            };
            // @TODO Redo this later
            RenderingDevice *device = RenderingDevice::get();
            vertex_buffer = device->create_buffer(&buffer_desc, "vertex_buffer");
            uint8_t *vb_ptr = device->map_buffer(vertex_buffer);
            std::memcpy(vb_ptr, vertices.data(), vertex_buffer_size);

            buffer_desc.size = index_buffer_size;
            buffer_desc.usage_flags = BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
            index_buffer = RenderingDevice::get()->create_buffer(&buffer_desc, "index_buffer");
            uint8_t *ib_ptr = device->map_buffer(index_buffer);
            std::memcpy(ib_ptr, indices.data(), index_buffer_size);
        }

        void destroy_render_data()
        {
            std::vector<BufferID> buffers;
            if (vertex_buffer.is_valid())
                buffers.push_back(vertex_buffer);
            if (index_buffer.is_valid())
                buffers.push_back(index_buffer);
            if (buffers.size() > 0)
                RenderingDevice::get()->destroy_buffers(buffers.data(), 2);
            vertex_buffer.id = K_INVALID_ID;
            index_buffer.id = K_INVALID_ID;
        }
    };

    struct TransformComponent
    {
        TransformComponent() : position(glm::vec3(0.0f)),
                               rotation(glm::fquat(1.0f, 0.0f, 0.0f, 0.0f)),
                               scale(glm::vec3(1.0f)),
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

        void update_local_transform()
        {
            if (dirty)
            {
                local_transform = glm::translate(glm::mat4(1.0f), position) *
                                  glm::mat4_cast(rotation) *
                                  glm::scale(glm::mat4(1.0f), scale);
                dirty = false;
            }
        }
    };

    struct Material
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