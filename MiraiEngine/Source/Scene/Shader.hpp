#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include "Common/Log.hpp"
namespace mirai {

    enum MeshType {
        MESH_TYPE_STATIC = 0,
        MESH_TYPE_DYNAMIC = 1,
    };

    enum PassMode : uint8_t {
        PASS_MODE_DEPTH_PREPASS = 0,
        PASS_MODE_FORWARD,
        PASS_MODE_DIRLIGHT_SHADOW,
        PASS_MODE_COUNT
    };

    enum DrawMode {
        DRAWMODE_INDEXED = 0,
        DRAWMODE_INDEXED_INDIRECT,
        DRAWMODE_INSTANCED,
        DRAWMODE_COUNT,
    };

    enum AlphaMode {
        ALPHA_MODE_OPAQUE = 0,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_MASK,
        ALPHA_MODE_MAX,
    };

    // Only material-facing variant bits are hashed.
    union MaterialKey {
        struct {
            uint32_t cull_mode : 2;
            uint32_t front_face : 1;
            uint32_t polygon_mode : 2;
            uint32_t alpha_mode : 2;
            uint32_t draw_mode : 3;
            uint32_t topology : 4;
            uint32_t depth_write : 1;
            uint32_t stencil_test : 1;
            uint32_t custom_shader : 1;
            uint32_t unused : 3;
        };
        struct {
            uint32_t hash;
        };

        struct {
            uint32_t custom_material_id : 16;
            uint32_t custom_material : 1;
            uint32_t unused : 3;
        };

        bool operator==(const MaterialKey &other) const {
            return hash == other.hash;
        }

        bool operator<=(const MaterialKey &other) const {
            return hash <= other.hash;
        }
    };

    inline uint32_t create_pso_key(PassMode pass, uint32_t mat_key, MeshType mesh_type) {
        // | PASS | MESH | MAT_KEY |
        //    8      4      20

        return cast_u32(pass) << 24 |
               cast_u32(mesh_type) << 20 |
               mat_key;
    }

    inline uint64_t create_sort_key(PassMode pass, uint32_t mat_key, MeshType mesh_type, BufferID buffer) {
        return uint64_t(create_pso_key(pass, mat_key, mesh_type)) << 32 |
               uint64_t(buffer.id);
    }

    struct PipelineState {
        CullMode cull_mode = CULL_MODE_BACK;
        FrontFace front_face = FRONT_FACE_COUNTER_CLOCKWISE;
        CompareOp depth_op = COMPARE_OP_LESS_OR_EQUAL;
        PolygonMode polygon_mode = POLYGON_MODE_FILL;
        Topology topology = TOPOLOGY_TRIANGLE_LIST;
        DrawMode draw_mode = DRAWMODE_INDEXED_INDIRECT;
        BlendMode blend_mode = BLEND_MODE_ADD;
        AlphaMode alpha_mode = ALPHA_MODE_OPAQUE;
        bool depth_test = true;
        bool depth_write = true;
        bool depth_bias = false;
        bool depth_clamp = false;
        bool stencil_test = false;

        uint16_t get_hash(PassMode pass, bool is_custom_shader) const {
            MaterialKey key = {};
            if (pass == PASS_MODE_DIRLIGHT_SHADOW && cull_mode != CULL_MODE_NONE) {
                key.cull_mode = alpha_mode == ALPHA_MODE_OPAQUE ? CULL_MODE_FRONT : CULL_MODE_NONE;
            } else {
                key.cull_mode = cull_mode;
            }

            key.front_face = front_face;
            key.polygon_mode = polygon_mode;
            key.alpha_mode = alpha_mode;
            key.draw_mode = draw_mode;
            key.topology = topology;
            key.depth_write = depth_write;
            key.stencil_test = stencil_test;
            key.custom_shader = is_custom_shader;
            return key.hash;
        }
    };

    struct PipelineAttachmentInfo {
        std::vector<Format> color_attachments_format = {};
        bool has_depth_attachment = false;
        Format depth_attachment_format;
    };

    static PipelineAttachmentInfo FORWARD_PASS_ATTACHMENTS = {
        .color_attachments_format = {FORMAT_R16G16B16A16_SFLOAT, FORMAT_R16G16_SFLOAT},
        .has_depth_attachment = true,
        .depth_attachment_format = FORMAT_D32_SFLOAT,
    };

    inline PipelineAttachmentInfo get_forward_pass_attachment_info() {
        return FORWARD_PASS_ATTACHMENTS;
    }

    struct Shader {
        std::string name;
        PipelineID pipeline_id;

        Shader(const Shader &) = delete;
        Shader(Shader &&) = delete;
        Shader operator=(const Shader &) = delete;
        Shader operator=(Shader &&) = delete;

        void bind(CommandBuffer *command_buffer);

        DrawMode get_draw_mode() {
            return draw_mode;
        }

        static std::shared_ptr<Shader> create_graphics_shader(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info);
        static std::shared_ptr<Shader> create_rt_shader(const std::string &name,
                                                        std::string ray_gen_shader_file,
                                                        std::vector<std::string> ray_hit_shader_files,
                                                        std::vector<std::string> ray_miss_shader_files,
                                                        uint32_t max_recursion_depth = 3);
        static std::shared_ptr<Shader> create_compute_shader(const std::string &name, const std::string &shader_file);

        ~Shader() {
        }

      private:
        Shader(const std::string &name) : name(name), pipeline_id(K_INVALID_ID) {
        }

        enum class ShaderType {
            Graphics,
            Compute,
            RayTracing
        };

        ShaderType shader_type;
        DrawMode draw_mode;
        std::vector<std::string> shader_files;
        PipelineAttachmentInfo attachment_info;
    };

} // namespace mirai