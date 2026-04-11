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
        ALPHA_MODE_OPAQUE,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_MASK,
    };

    union MaterialKey {
        struct {
            uint32_t cull_mode : 2;
            uint32_t front_face : 1;
            uint32_t depth_op : 3;
            uint32_t polygon_mode : 2;
            uint32_t topology : 4;
            uint32_t draw_mode : 2;
            uint32_t blend_mode : 3;
            uint32_t depth_test : 1;
            uint32_t depth_write : 1;
            uint32_t depth_bias : 1;
            uint32_t depth_clamp : 1;
            uint32_t stencil_test : 1;
            uint32_t alpha_mode : 2;
            uint32_t padding : 8;
        };
        struct {
            uint32_t hash;
        };

        bool operator==(const MaterialKey &other) const {
            return hash == other.hash;
        }

        bool operator<=(const MaterialKey &other) const {
            return hash <= other.hash;
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

        inline uint32_t get_hash() const {
            MaterialKey key;
            key.cull_mode = cull_mode;
            key.front_face = front_face;
            key.depth_op = depth_op;
            key.polygon_mode = polygon_mode;
            key.topology = topology;
            key.draw_mode = draw_mode;
            key.blend_mode = blend_mode;
            key.alpha_mode = alpha_mode;
            key.depth_test = depth_test;
            key.depth_write = depth_write;
            key.depth_bias = depth_bias;
            key.depth_clamp = depth_clamp;
            key.stencil_test = stencil_test;
            key.padding = 0;
            return key.hash;
        }
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