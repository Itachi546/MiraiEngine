#pragma once

#include "ECS.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Math/Math.hpp"

namespace mirai {
    struct NameComponent {
        std::string name;
    };

    struct HierarchyComponent {
        std::vector<Entity> childrens;
        Entity parent;

        void set_parent(Entity parent) {
            this->parent = parent;
        }

        void add_children(Entity children) {
            auto found = std::find(childrens.begin(), childrens.end(), children);
            if (found == childrens.end())
                childrens.push_back(children);
            else
                Log::Warn("Trying to add duplicate children");
        }

        void remove_children(Entity children) {
            auto found = std::find(childrens.begin(), childrens.end(), children);
            if (found != childrens.end()) {
                childrens.erase(found);
            }
        }

        void clear_parent() {
            this->parent = K_INVALID_ENTITY;
        }
    };

    struct BufferView {
        uint32_t offset;
        uint32_t count;
    };

    struct Vertex {
        float px, py, pz;
        uint32_t normal;

        uint32_t tangent;
        uint32_t bitangent;
        float tu, tv;
    };

    static_assert(sizeof(Vertex) % 16 == 0);

    struct MeshComponent {
        enum FLAGS {
            EMPTY = 0,
            RENDERABLE = 1 << 0,
            DYNAMIC = 1 << 2,
            CAST_SHADOW = 1 << 2,
            RECEIVE_SHADOW = 1 << 3,
            DEPTH_TEST = 1 << 4,
            DEPTH_WRITE = 1 << 5
        };

        uint32_t _flags = RENDERABLE | DEPTH_TEST | DEPTH_WRITE | CAST_SHADOW | RECEIVE_SHADOW;

        struct MeshSubset {
            BufferView vertex_buffer;
            BufferView index_buffer;
            uint32_t vertex_count;
            uint32_t material_index;
            UniformSetID vertex_binding_set;
        };

        uint32_t gpu_mesh_index;
        std::vector<MeshSubset> mesh_subsets;
        std::vector<AABB> aabbs;
    };

    struct TransformComponent {
        TransformComponent() : position(glm::vec3(0.0f)),
                               rotation(glm::fquat(1.0f, 0.0f, 0.0f, 0.0f)),
                               scale(glm::vec3(1.0f)),
                               local_transform(glm::mat4(1.0f)),
                               world_transform(glm::mat4(1.0f)),
                               dirty(true) {
        }

        glm::vec3 position;
        bool dirty;

        glm::fquat rotation;
        glm::vec3 scale;

        // @TODO: Create only one transform
        // Bake world_transform into position, rotation and scale as well as AABB
        glm::mat4 local_transform;
        glm::mat4 world_transform;

        void update_local_transform() {
            if (dirty) {
                local_transform = glm::translate(glm::mat4(1.0f), position) *
                                  glm::mat4_cast(rotation) *
                                  glm::scale(glm::mat4(1.0f), scale);
            }
        }

        // Increment current rotation by given euler angles
        void rotate(glm::vec3 eulerAngles) {
            glm::fquat delta{eulerAngles};
            rotation *= delta;
            rotation = glm::normalize(rotation);
            dirty = true;
        }
    };

    enum LightType {
        LIGHT_TYPE_DIRECTIONAL = 0,
        LIGHT_TYPE_POINT = 1,
        LIGHT_TYPE_SPOT = 2
    };

    struct Light {
        union {
            glm::vec3 position;
            glm::vec3 rotation;
        };

        inline glm::vec3 get_direction() {
            return rotation_to_direction(glm::radians(rotation));
        }

        LightType light_type;
        glm::vec3 color;
        float intensity;

        bool cast_shadow;
    };

    struct Material {
        enum FLAGS {
            FLAG_EMPTY = 0,
            FLAG_OPAQUE = 1 << 0,
            FLAG_ALPHA_BLEND = 1 << 1,
            FLAG_ALPHA_MASK = 1 << 2,
            FLAG_DOUBLE_SIDED = 1 << 3,
            FLAG_SPECULAR_GLOSSINESS_WORKFLOW = 1 << 4,
        };

        glm::vec4 albedo;

        glm::vec3 emissive_factor;
        float metallic_factor;

        float roughness_factor;
        float transmission;
        uint32_t flags = 0;
        uint32_t emissive_texture;

        uint32_t albedo_texture;
        uint32_t normal_texture;
        uint32_t metallic_roughness_texture;
        uint32_t occlusion_texture;

        bool is_transparent() const {
            return ((flags & FLAG_ALPHA_BLEND) == FLAG_ALPHA_BLEND) || ((flags & FLAG_ALPHA_MASK) == FLAG_ALPHA_MASK);
        }
    };
    static_assert(sizeof(Material) % 16 == 0);
}; // namespace mirai