#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
namespace mirai {

    enum DrawMode {
        DRAWMODE_INDEXED = 0,
        DRAWMODE_INDEXED_INDIRECT,
        DRAWMODE_INSTANCED,
    };

    enum AlphaMode {
        ALPHA_MODE_OPAQUE = 0,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_MASK,
        ALPHA_MODE_MAX,
    };

    // Behavioral flags — not part of the sort key.
    enum RenderFlags : uint32_t {
        RENDER_FLAG_NONE = 0,
        RENDER_FLAG_CAST_SHADOW = 1 << 0,
        RENDER_FLAG_RECEIVE_SHADOW = 1 << 1,
        RENDER_FLAG_DEFAULT = RENDER_FLAG_CAST_SHADOW | RENDER_FLAG_RECEIVE_SHADOW,
    };

    // Only material-facing variant bits are hashed.
    union MaterialKey {
        struct {
            uint16_t cull_mode : 2;    // affects pipeline variant
            uint16_t front_face : 1;   // affects pipeline variant
            uint16_t polygon_mode : 2; // affects pipeline variant (wireframe)
            uint16_t alpha_mode : 2;   // opaque / blend / mask
            uint16_t padding : 9;
        };
        struct {
            uint16_t hash;
        };

        bool operator==(const MaterialKey &other) const {
            return hash == other.hash;
        }

        bool operator<=(const MaterialKey &other) const {
            return hash <= other.hash;
        }
    };

    struct MaterialOverrides {
        uint16_t override_flag = 0;
        uint16_t override_value = 0;

        void set_cull_mode(CullMode cull_mode) {
            override_flag |= 3;
            override_value |= uint16_t(cull_mode);
        }

        void set_front_face(FrontFace front_face) {
            override_flag |= 4;
            override_value |= uint16_t(front_face);
        }

        void set_polygon_mode(PolygonMode polygon_mode) {
            override_flag |= 24;
            override_value |= uint16_t(polygon_mode);
        }

        void set_alpha_mode(AlphaMode alpha_mode) {
            override_flag |= 96;
            override_value |= uint16_t(alpha_mode);
        }
    };

    // Material-facing properties — drives sort key and shader variant selection.
    // render_flags is behavioral and intentionally NOT part of the hash.
    struct MaterialState {
        CullMode cull_mode = CULL_MODE_BACK;
        FrontFace front_face = FRONT_FACE_COUNTER_CLOCKWISE;
        PolygonMode polygon_mode = POLYGON_MODE_FILL;
        AlphaMode alpha_mode = ALPHA_MODE_OPAQUE;
        uint32_t render_flags = RENDER_FLAG_DEFAULT;

        uint16_t get_hash() const {
            MaterialKey key = {};
            key.cull_mode = cull_mode;
            key.front_face = front_face;
            key.polygon_mode = polygon_mode;
            key.alpha_mode = alpha_mode;
            key.padding = 0;
            return key.hash;
        }

        bool has_flag(RenderFlags flag) const {
            return (render_flags & flag) == flag;
        }
    };

    struct PipelineState {
        CullMode cull_mode = CULL_MODE_BACK;
        FrontFace front_face = FRONT_FACE_COUNTER_CLOCKWISE;
        CompareOp depth_op = COMPARE_OP_LESS_OR_EQUAL;
        PolygonMode polygon_mode = POLYGON_MODE_FILL;
        Topology topology = TOPOLOGY_TRIANGLE_LIST;
        DrawMode draw_mode = DRAWMODE_INDEXED;
        BlendMode blend_mode = BLEND_MODE_ADD;
        AlphaMode alpha_mode = ALPHA_MODE_OPAQUE;
        bool depth_test = false;
        bool depth_write = false;
        bool depth_bias = false;
        bool depth_clamp = false;
        bool stencil_test = false;

        // Note: PipelineState has no get_hash() — it is only used at shader
        // creation time. Sort keys are derived from MaterialState::get_hash().
    };

    struct PipelineAttachmentInfo {
        std::vector<Format> color_attachments_format = {};
        bool has_depth_attachment = false;
        Format depth_attachment_format;
    };

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

        static std::shared_ptr<Shader> create_from_file(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info);
        static std::shared_ptr<Shader> create_from_file(const std::string &name, const std::string &shader_file);

      private:
        Shader(const std::string &name) : name(name), pipeline_id(K_INVALID_ID), is_graphics_shader(false) {
        }

        bool is_graphics_shader;
        DrawMode draw_mode;
        std::vector<std::string> shader_files;
        PipelineAttachmentInfo attachment_info;
    };

} // namespace mirai