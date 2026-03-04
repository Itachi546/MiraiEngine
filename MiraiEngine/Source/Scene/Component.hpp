#pragma once

#include "ECS.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Math/Angles.hpp"

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

    struct MeshComponent {
        enum FLAGS {
            EMPTY = 0,
            RENDERABLE = 1 << 0,
            DYNAMIC = 1 << 2,
            CAST_SHADOW = 1 << 2,
            RECEIVE_SHADOW = 1 << 3,
            DEPTH_TEST = 1 << 4,
            DEPTH_WRITE = 1 << 5,
            DISABLE_FRUSTUM_CULLING = 1 << 6,
            SKINNED = 1 << 7,
        };

        uint32_t _flags = RENDERABLE | DEPTH_TEST | DEPTH_WRITE | CAST_SHADOW | RECEIVE_SHADOW;
        BufferView vertex_buffer;
        BufferView index_buffer;

        struct MeshSubset {
            uint32_t vertex_offset_bytes;
            uint32_t index_offset_bytes;
            uint32_t index_count;
            uint32_t vertex_stride;
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
                local_transform = get_local_transform();
            }
        }

        glm::mat4 get_local_transform() {
            /*
            glm::mat4 result = glm::mat4(1.0f);
            result = glm::translate(result, position);
            result = result * glm::mat4_cast(rotation);
            result = glm::scale(result, scale);
            return result;
            */
            return glm::translate(position) * glm::mat4_cast(rotation) * glm::scale(scale);
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

    struct LightComponent {
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

    struct NodeAnimatorComponent {
        // Index to the array of AnimationClip stored in scene
        uint32_t current_clip_index;
        float current_time = 0.0f;
        float playback_speed = 1.0f;
        bool looping = true;
        bool should_update_children = true;

        float update() {
        }
    };

}; // namespace mirai