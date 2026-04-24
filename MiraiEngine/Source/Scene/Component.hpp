#pragma once

#include "ECS.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Math/Angles.hpp"

namespace mirai {
    struct NameComponent {
        std::string name;
    };

    struct HierarchyComponent {
        Entity parent;
        std::vector<Entity> childrens;

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

    enum MeshType {
        MESH_TYPE_STATIC = 0,
        MESH_TYPE_DYNAMIC = 1,
        MESH_TYPE_SKINNED = 2,
    };

    struct MeshComponent {
        MeshType mesh_type;
        BufferView vertex_buffer;
        BufferView index_buffer;

        struct MeshSubset {
            uint32_t vertex_offset_bytes;
            uint32_t vertex_count;
            uint32_t index_offset_bytes;
            uint32_t index_count;
            uint32_t vertex_stride;
            uint32_t material_index;
        };

        uint32_t gpu_mesh_index;
        std::vector<MeshSubset> mesh_subsets;
        std::vector<AABB> aabbs;
    };

    struct TransformComponent {
        TransformComponent() : position(glm::vec3(0.0f)),
                               dirty(true),
                               rotation(glm::fquat(1.0f, 0.0f, 0.0f, 0.0f)),
                               scale(glm::vec3(1.0f)),
                               local_transform(glm::mat4(1.0f)),
                               world_transform(glm::mat4(1.0f)) {
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
        LightType light_type;
        glm::vec3 color;
        float intensity;
        union {
            float radius;
            float height;
        };
        bool cast_shadow;

        float inner_cone_angle;
        float outer_cone_angle;
    };

    struct NodeAnimatorComponent {
        // Index to the array of AnimationClip stored in scene
        int current_animation_clip;
        float current_time = 0.0f;
        float playback_speed = 1.0f;
        bool looping = true;
        bool should_update_children = true;
    };

    struct AnimatorComponent {
        int current_animation_clip;
        uint32_t skeleton_index;
        float current_time = 0.0f;
    };

}; // namespace mirai