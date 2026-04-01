#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"

namespace mirai {

    enum ShaderPass {
        SHADER_PASS_DEPTH_PREPASS,
        SHADER_PASS_PBR_FORWARD,
        SHADER_PASS_PBR_FORWARD_TRANSPARENT,
        SHADER_PASS_FORWARD_UNLIT,
        SHADER_PASS_FORWARD_UNLIT_TRANSPARENT,
        SHADER_PASS_SKYBOX,
        SHADER_PASS_PBR_DEFERRED,
        SHADER_PASS_PBR_DEFERRED_SKINNED,
        SHADER_PASS_PBR_DEFERRED_ALPHA,
        SHADER_PASS_PBR_DEFERRED_TRANSPARENT,
        SHADER_PASS_DEFERRED_UNLIT,
        SHADER_PASS_DEFERRED_UNLIT_TRANSPARENT,
        SHADER_PASS_CASCADED_SHADOW,
        SHADER_PASS_CASCADED_SHADOW_ALPHA_MASK,
        SHADER_PASS_TEXT2D,
        SHADER_PASS_DEBUG_DRAW,
        SHADER_PASS_COUNT
    };

    enum DrawMode {
        DRAWMODE_INDEXED = 0,
        DRAWMODE_INDEXED_INDIRECT,
        DRAWMODE_INSTANCED,
    };

    struct PipelineState {
        union PipelineRenderState {
            struct {
                uint64_t cull_mode : 2;    // 2
                uint64_t front_face : 1;   // 3
                uint64_t depth_test : 1;   // 4
                uint64_t depth_write : 1;  // 5
                uint64_t depth_clamp : 1;  // 6
                uint64_t depth_bias : 1;   // 7
                uint64_t blend_mode : 1;   // 8
                uint64_t depth_op : 3;     // 11
                uint64_t topology : 4;     // 15
                uint64_t polygon_mode : 2; // 17
                uint64_t pass_mode : 16;   // 33
                uint64_t draw_mode : 2;    // 35
                uint64_t _reserved : 29;
            } fields;
            uint64_t hash;
        } render_state;
        uint32_t custom_shader_id = 0;

        PipelineState() {
            render_state.fields.cull_mode = CULL_MODE_BACK;
            render_state.fields.front_face = FRONT_FACE_COUNTER_CLOCKWISE;
            render_state.fields.depth_test = false;
            render_state.fields.depth_write = false;
            render_state.fields.depth_clamp = false;
            render_state.fields.blend_mode = false;
            render_state.fields.depth_bias = false;
            render_state.fields.depth_op = COMPARE_OP_LESS_OR_EQUAL;
            render_state.fields.topology = TOPOLOGY_TRIANGLE_LIST;
            render_state.fields.polygon_mode = POLYGON_MODE_FILL;
            render_state.fields.draw_mode = DRAWMODE_INDEXED;
            render_state.fields._reserved = 0;
            render_state.fields.pass_mode = SHADER_PASS_COUNT;
        }

        uint64_t get_hash() const {
            uint64_t hash = custom_shader_id;
            utils::hash_combine(hash, render_state.hash);
            return hash;
        }
    };

    struct PipelineAttachmentInfo {
        std::vector<Format> color_attachments_format;
        bool has_depth_attachment = false;
        Format depth_attachment_format;
    };

    struct Shader {
        Shader(const std::string &name) : name(name), pipeline_id(K_INVALID_ID) {
        }

        std::string name;
        PipelineID pipeline_id;

        void bind(CommandBuffer *command_buffer);

        DrawMode get_draw_mode() {
            return draw_mode;
        }

        static Shader *create_from_file(const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, const std::vector<std::string> &shader_files, const std::string &name);
        static Shader *create_from_file(const std::string &shader_file, const std::string &name);

        static uint32_t create_shader_id() {
            static uint32_t id = 1;
            return id++;
        }

      private:
        DrawMode draw_mode;
    };

} // namespace mirai