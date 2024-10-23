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
    };

    struct MeshComponent
    {
        BufferID vertex_buffer;
        BufferID index_buffer;

        uint32_t vertex_buffer_size;
        uint32_t index_buffer_size;
    };

    struct BufferView
    {
        uint32_t offset;
        uint32_t size;
    };

    struct TransformComponent 
    {
        glm::vec3 position;
        bool dirty;

        glm::fquat rotation;

        glm::mat4 local_transform;
        glm::mat4 world_transform;
    };

    constexpr const uint8_t CULL_MODE_FLAG = 62;
    constexpr const uint8_t DEPTH_TEST_FLAG = 61; 
    constexpr const uint8_t DEPTH_WRITE_FLAG = 60; 
    constexpr const uint8_t FRONT_FACE_FLAG = 59; 
    struct ObjectComponent
    {
        MeshComponent* mesh_component;
        BufferView vertex_buffer;
        BufferView index_buffer;
        uint32_t vertex_count;

        CullMode cull_mode = CULL_MODE_BACK;
        bool enable_depth_test = true;
        bool enable_depth_write = true;
        FrontFace front_face = FRONT_FACE_COUNTER_CLOCKWISE;

        uint32_t pipeline_hash;
    };

    struct MaterialComponent {
        uint32_t pipeline_id;
        std::string name;
    };
}; // namespace mirai