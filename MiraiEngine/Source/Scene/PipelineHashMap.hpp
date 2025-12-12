#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include <unordered_map>
#include <vector>

namespace mirai {
    class ShaderMaterial;

    enum ShaderPass {
        SHADER_PASS_DEPTH_PREPASS,
        SHADER_PASS_PBR_FORWARD,
        SHADER_PASS_PBR_FORWARD_TRANSPARENT,
        SHADER_PASS_FORWARD_UNLIT,
        SHADER_PASS_FORWARD_UNLIT_TRANSPARENT,
        SHADER_PASS_SKYBOX,
        SHADER_PASS_PBR_DEFERRED,
        SHADER_PASS_PBR_DEFERRED_TRANSPARENT,
        SHADER_PASS_DEFERRED_UNLIT,
        SHADER_PASS_DEFERRED_UNLIT_TRANSPARENT,
        SHADER_PASS_SHADOW,
        SHADER_PASS_COUNT
    };
    struct PipelineState {
        union PipelineRenderState {
            struct {
                uint64_t cull_mode : 2;    // 2
                uint64_t front_face : 1;   // 3
                uint64_t depth_test : 1;   // 4
                uint64_t depth_write : 1;  // 5
                uint64_t depth_clamp : 1;  // 6
                uint64_t blend_mode : 1;   // 7
                uint64_t depth_op : 3;     // 10
                uint64_t topology : 4;     // 14
                uint64_t polygon_mode : 2; // 16
                uint64_t pass_mode : 16;   // 32
                uint64_t _reserved : 32;
            } fields;
            uint64_t hash;
        } render_state;
        uint32_t custom_shader_id = 0;
        // Can have maximum upto 16 attachment format
        Format color_attachment_formats[16];
        uint8_t color_attachment_count;
        bool has_depth_attachment;
        Format depth_attachment_format;

        PipelineState() {
            render_state.fields.cull_mode = CULL_MODE_BACK;
            render_state.fields.front_face = FRONT_FACE_COUNTER_CLOCKWISE;
            render_state.fields.depth_test = false;
            render_state.fields.depth_write = false;
            render_state.fields.depth_clamp = false;
            render_state.fields.blend_mode = false;
            render_state.fields.depth_op = COMPARE_OP_LESS_OR_EQUAL;
            render_state.fields.topology = TOPOLOGY_TRIANGLE_LIST;
            render_state.fields.polygon_mode = POLYGON_MODE_FILL;
            render_state.fields._reserved = 0;
            color_attachment_count = 0;
            has_depth_attachment = false;
        }

        uint64_t get_hash() const {
            uint64_t hash = custom_shader_id;
            utils::hash_combine(hash, render_state.hash);
            return hash;
        }
    };

    class PipelineHashMap {

      public:
        PipelineHashMap();
        PipelineHashMap(const PipelineHashMap &) = delete;
        void operator=(const PipelineHashMap &) = delete;

        ~PipelineHashMap();

        PipelineID add_or_get_graphics_pipeline(const PipelineState &pipeline_state, const std::vector<std::string> &shader_files, const std::string &name);
        PipelineID add_or_get_compute_pipeline(const std::string &shader_files, const std::string &name);

        PipelineID get_from_state_hash(uint64_t hash);

        static PipelineHashMap *get() {
            return Instance;
        }

        void destroy();

      private:
        static PipelineHashMap *Instance;
        std::unordered_map<uint64_t, PipelineID> pipeline_map;
    };

} // namespace mirai