#pragma once

#include "Math/Math.hpp"
#include "Math/MathUtils.hpp"

namespace mirai {
    struct VertexData {
        glm::vec3 position;
        uint32_t normal;

        uint32_t tangent;
        uint32_t bitangent;
        glm::vec2 uv;
    };

    inline VertexData create_vertex(const glm::vec3 &position, const glm::vec3 &normal, const glm::vec3 &tangent, const glm::vec3 &bitangent, const glm::vec2 &uv) {
        VertexData data;
        data.position = position;
        data.normal = utils::pack_vec3_to_u32(normal.x, normal.y, normal.z);
        data.tangent = utils::pack_vec3_to_u32(tangent.x, tangent.y, tangent.z);
        data.bitangent = utils::pack_vec3_to_u32(bitangent.x, bitangent.y, bitangent.z);
        data.uv = uv;
        return data;
    }

    inline VertexData create_vertex(const glm::vec3 &position, const glm::vec3 &normal, const glm::vec2 &uv) {
        VertexData data;
        data.position = position;
        data.normal = utils::pack_vec3_to_u32(normal.x, normal.y, normal.z);
        data.tangent = 0;
        data.bitangent = 0;
        data.uv = uv;
        return data;
    }

    const uint32_t K_VERTEX_DATA_SIZE = sizeof(VertexData);
    struct SkinnedVertexData {
        glm::vec3 position;
        uint32_t normal;

        uint32_t tangent;
        uint32_t bitangent;
        glm::vec2 uv;

        uint32_t joints;
        glm::vec4 weights;
    };

    const uint32_t K_VERTEX_DATA_SIZE_SKINNED = sizeof(SkinnedVertexData);

    struct MeshData {
        std::vector<VertexData> vertices;
        std::vector<uint32_t> indices;
    };

    void generate_tangent_space(MeshData *mesh_data);

    void generate_plane_mesh(MeshData *out_mesh_data);
    void generate_cube_mesh(MeshData *out_mesh_data);
    void generate_sphere_mesh(MeshData *out_mesh_data, int segments = 32, int rings = 16);

} // namespace mirai