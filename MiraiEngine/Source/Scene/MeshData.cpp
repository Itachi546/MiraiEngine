#include "MeshData.hpp"

#include "mikktspace.h"
#include "Common/CommonInclude.hpp"

namespace mirai {
    struct MikkTSpaceUserData {
        MeshData *mesh;
    };

    static int get_num_faces(const SMikkTSpaceContext *context) {
        const MikkTSpaceUserData *user_data = static_cast<const MikkTSpaceUserData *>(context->m_pUserData);
        return cast_int(user_data->mesh->indices.size()) / 3;
    }

    static int get_num_vertices_of_face(const SMikkTSpaceContext *context, const int iface) {
        return 3;
    }

    static int get_vertex_index(const SMikkTSpaceContext *context, int face, int vert) {
        const MikkTSpaceUserData *user_data = static_cast<const MikkTSpaceUserData *>(context->m_pUserData);
        int face_size = get_num_vertices_of_face(context, face);
        int indices_index = face * face_size + vert;
        return user_data->mesh->indices[indices_index];
    }

    static void get_position(const SMikkTSpaceContext *context, float pos_out[], const int face, const int vert) {
        const MikkTSpaceUserData *user_data = static_cast<const MikkTSpaceUserData *>(context->m_pUserData);
        int index = get_vertex_index(context, face, vert);
        const glm::vec3 &position = user_data->mesh->vertices[index].position;
        pos_out[0] = position.x, pos_out[1] = position.y, pos_out[2] = position.z;
    }

    static void get_normal(const SMikkTSpaceContext *context, float norm_out[], const int face, const int vert) {
        const MikkTSpaceUserData *user_data = static_cast<const MikkTSpaceUserData *>(context->m_pUserData);
        int index = get_vertex_index(context, face, vert);
        utils::u32_to_vec3(user_data->mesh->vertices[index].normal, norm_out);
    }

    static void get_tex_coord(const SMikkTSpaceContext *context, float tex_coord[], const int face, const int vert) {
        const MikkTSpaceUserData *user_data = static_cast<const MikkTSpaceUserData *>(context->m_pUserData);

        int index = get_vertex_index(context, face, vert);
        const glm::vec2 &uv = user_data->mesh->vertices[index].uv;
        tex_coord[0] = uv.x;
        tex_coord[1] = uv.y;
    }

    static void set_tspace_basic(const SMikkTSpaceContext *context, const float *tangentu, const float sign, const int face, const int vert) {
        MikkTSpaceUserData *user_data = static_cast<MikkTSpaceUserData *>(context->m_pUserData);

        int index = get_vertex_index(context, face, vert);
        auto &vertex_data = user_data->mesh->vertices[index];

        glm::vec3 normal;
        utils::u32_to_vec3(vertex_data.normal, &normal[0]);
        glm::vec3 tangent = glm::vec3(tangentu[0], tangentu[1], tangentu[2]);
        glm::vec3 bitangent = sign * glm::cross(normal, tangent);

        vertex_data.tangent = utils::pack_vec3_to_u32(tangent.x, tangent.y, tangent.z);
        vertex_data.bitangent = utils::pack_vec3_to_u32(bitangent.x, bitangent.y, bitangent.z);
    }

    void generate_tangent_space(MeshData *mesh_data) {
        MikkTSpaceUserData user_data;
        user_data.mesh = mesh_data;

        SMikkTSpaceInterface interface = {};
        interface.m_getNumFaces = get_num_faces;
        interface.m_getNumVerticesOfFace = get_num_vertices_of_face;
        interface.m_getPosition = get_position;
        interface.m_getNormal = get_normal;
        interface.m_getTexCoord = get_tex_coord;
        interface.m_setTSpaceBasic = set_tspace_basic;

        SMikkTSpaceContext context = {};
        context.m_pInterface = &interface;
        context.m_pUserData = &user_data;

        tbool result = genTangSpaceDefault(&context);
        ASSERT(result == 1);
    }

    void generate_plane_mesh(MeshData *out_mesh_data) {
        out_mesh_data->vertices = {
            create_vertex(glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(-0.5f, 0.0f, +0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)),
            create_vertex(glm::vec3(+0.5f, 0.0f, +0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(+0.5f, 0.0f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
        };

        out_mesh_data->indices = {
            0,
            1,
            3,
            3,
            1,
            2,
        };

        generate_tangent_space(out_mesh_data);
    }

    void generate_cube_mesh(MeshData *out_mesh_data) {
        out_mesh_data->vertices = {
            // Front face (+Z) — normal: (0, 0, 1)
            create_vertex(glm::vec3(-0.5f, -0.5f, +0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, -0.5f, +0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, +0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(-0.5f, +0.5f, +0.5f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec2(0.0f, 1.0f)),

            // Back face (-Z) — normal: (0, 0, -1)
            create_vertex(glm::vec3(+0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(-0.5f, +0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, -0.5f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec2(0.0f, 1.0f)),

            // Right face (+X) — normal: (1, 0, 0)
            create_vertex(glm::vec3(+0.5f, -0.5f, +0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, -0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, -0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, +0.5f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)),

            // Left face (-X) — normal: (-1, 0, 0)
            create_vertex(glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(-0.5f, -0.5f, +0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(-0.5f, +0.5f, +0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(-0.5f, +0.5f, -0.5f), glm::vec3(-1.0f, 0.0f, 0.0f), glm::vec2(0.0f, 1.0f)),

            // Top face (+Y) — normal: (0, 1, 0)
            create_vertex(glm::vec3(-0.5f, +0.5f, +0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, +0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, +0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(-0.5f, +0.5f, -0.5f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec2(0.0f, 1.0f)),

            // Bottom face (-Y) — normal: (0, -1, 0)
            create_vertex(glm::vec3(-0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, -0.5f, -0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 0.0f)),
            create_vertex(glm::vec3(+0.5f, -0.5f, +0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(1.0f, 1.0f)),
            create_vertex(glm::vec3(-0.5f, -0.5f, +0.5f), glm::vec3(0.0f, -1.0f, 0.0f), glm::vec2(0.0f, 1.0f)),
        };

        out_mesh_data->indices = {
            // Front
            0,
            1,
            2,
            0,
            2,
            3,
            // Back
            4,
            5,
            6,
            4,
            6,
            7,
            // Right
            8,
            9,
            10,
            8,
            10,
            11,
            // Left
            12,
            13,
            14,
            12,
            14,
            15,
            // Top
            16,
            17,
            18,
            16,
            18,
            19,
            // Bottom
            20,
            21,
            22,
            20,
            22,
            23,
        };
        generate_tangent_space(out_mesh_data);
    }

    void generate_sphere_mesh(MeshData *out_mesh_data, int segments, int rings) {
        out_mesh_data->vertices.clear();
        out_mesh_data->indices.clear();

        for (int r = 0; r <= rings + 1; ++r) {
            float v = (float)r / (float)(rings + 1); // 0 (top) → 1 (bottom)
            float polar = glm::pi<float>() * v;      // 0 → PI
            float sin_p = glm::sin(polar);
            float cos_p = glm::cos(polar);

            for (int s = 0; s <= segments; ++s) {
                float u = (float)s / (float)segments;  // 0 → 1
                float azim = glm::two_pi<float>() * u; // 0 → 2PI
                float sin_a = glm::sin(azim);
                float cos_a = glm::cos(azim);

                glm::vec3 normal = glm::vec3(sin_p * cos_a,  // x
                                             cos_p,          // y
                                             sin_p * sin_a); // z

                glm::vec3 position = normal * 0.5f; // unit sphere: -0.5 to +0.5

                glm::vec2 uv = glm::vec2(u, v);

                out_mesh_data->vertices.push_back(
                    create_vertex(position, normal, uv));
            }
        }

        // Indices — CCW winding when viewed from outside
        int row_size = segments + 1;

        for (int r = 0; r < rings + 1; ++r) {
            for (int s = 0; s < segments; ++s) {
                int tl = (r)*row_size + s;           // top-left
                int tr = (r)*row_size + s + 1;       // top-right
                int bl = (r + 1) * row_size + s;     // bottom-left
                int br = (r + 1) * row_size + s + 1; // bottom-right

                out_mesh_data->indices.push_back(tl);
                out_mesh_data->indices.push_back(tr);
                out_mesh_data->indices.push_back(bl);

                out_mesh_data->indices.push_back(tr);
                out_mesh_data->indices.push_back(br);
                out_mesh_data->indices.push_back(bl);
            }
        }
    }

} // namespace mirai