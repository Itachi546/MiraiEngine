#include "PipelineLoader.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Engine/Log.hpp"
#include "Scene/Material.hpp"
#include "Scene/Component.hpp"
#include "Common/JobSystem.hpp"

namespace mirai {
    // Register a shader under a material sort key for a given pass.
    // PipelineState is used only to compile the GPU pipeline and is discarded after.
    void create_shader_material(const std::string &name,
                                PassMode pass_mode,
                                const std::vector<std::string> &shaders,
                                const PipelineState &pipeline_state,
                                const PipelineAttachmentInfo &attachment_info,
                                MeshType mesh_type = MESH_TYPE_STATIC) {

        uint32_t pso_key = create_pso_key(pass_mode, pipeline_state.get_hash(pass_mode), mesh_type);

        ShaderRegistry *registry = ShaderRegistry::get();
        if (registry->has(pso_key)) {
            Log::Warn("Shader variant already exist");
            ASSERT(0);
            return;
        }
        std::shared_ptr<Shader> shader = Shader::create_from_file(name, shaders, pipeline_state, attachment_info);
        registry->add(pso_key, shader);
    }

    void preload_shaders() {
        // ── Depth Pre-Pass ─────────────────────────────────────────────────────
        // Can run these shader generation in parallel right now, due to the fact that ID generation is not thread safe
        create_shader_material("depth_prepass", PASS_MODE_DEPTH_PREPASS,
                               {"SPIRV/depth-prepass.vert.spv"},
                               PipelineState{
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});

        create_shader_material("depth_prepass", PASS_MODE_DEPTH_PREPASS,
                               {"SPIRV/depth-prepass.vert.spv"},
                               PipelineState{
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT}, MESH_TYPE_DYNAMIC);

        create_shader_material("depth_prepass_double_sided", PASS_MODE_DEPTH_PREPASS,
                               {"SPIRV/depth-prepass.vert.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_NONE,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});
        create_shader_material("depth_prepass_alpha_mask", PASS_MODE_DEPTH_PREPASS,
                               {"SPIRV/depth-prepass-alpha.vert.spv", "SPIRV/depth-prepass-alpha.frag.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_NONE,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .alpha_mode = ALPHA_MODE_MASK,
                                   .depth_test = true,
                                   .depth_write = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});

        // ── Cascaded Shadow Pass ────────────────────────────────────────────────
        create_shader_material("cascaded_shadow", PASS_MODE_DIRLIGHT_SHADOW,
                               {"SPIRV/cascaded-shadow.vert.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_FRONT,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                                   .depth_clamp = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D16_UNORM});
        create_shader_material("cascaded_shadow", PASS_MODE_DIRLIGHT_SHADOW,
                               {"SPIRV/cascaded-shadow.vert.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_NONE,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                                   .depth_clamp = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D16_UNORM});

        create_shader_material("cascaded_shadow", PASS_MODE_DIRLIGHT_SHADOW,
                               {"SPIRV/cascaded-shadow.vert.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_FRONT,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .depth_test = true,
                                   .depth_write = true,
                                   .depth_clamp = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D16_UNORM},
                               MESH_TYPE_DYNAMIC);

        create_shader_material("cascaded_shadow_alpha_mask", PASS_MODE_DIRLIGHT_SHADOW,
                               {"SPIRV/cascaded-shadow-alpha.vert.spv", "SPIRV/cascaded-shadow-alpha.frag.spv"},
                               PipelineState{
                                   .cull_mode = CULL_MODE_NONE,
                                   .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                   .alpha_mode = ALPHA_MODE_MASK,
                                   .depth_test = true,
                                   .depth_write = true,
                                   .depth_clamp = true,
                               },
                               {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D16_UNORM});

        if (AppSettings::render_mode == RenderMode::RENDERMODE_FORWARD) {
            Format color_format = FORMAT_R16G16B16A16_SFLOAT;
            PipelineAttachmentInfo fwd_attachment = {
                .color_attachments_format = {color_format, FORMAT_R16G16_SFLOAT},
                .has_depth_attachment = true,
                .depth_attachment_format = FORMAT_D32_SFLOAT,
            };
            // ── Forward Pass ────────────────────────────────────────────────────
            // depth_op = EQUAL: depth prepass already wrote depth; the pass owns this.
            create_shader_material("forward-pass", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass.frag.spv"},
                                   PipelineState{
                                       .depth_op = COMPARE_OP_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .depth_test = true,
                                       .depth_write = true,
                                   },
                                   fwd_attachment);
            create_shader_material("forward-pass", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass.frag.spv"},
                                   PipelineState{
                                       .depth_op = COMPARE_OP_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .depth_test = true,
                                       .depth_write = true,
                                   },
                                   fwd_attachment, MESH_TYPE_DYNAMIC);
            create_shader_material("forward-pass-double-sided", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass.frag.spv"},
                                   PipelineState{
                                       .cull_mode = CULL_MODE_NONE,
                                       .depth_op = COMPARE_OP_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .depth_test = true,
                                       .depth_write = true,
                                   },
                                   fwd_attachment);
            create_shader_material("forward-pass-alpha-mode", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass-alpha.frag.spv"},
                                   PipelineState{
                                       .cull_mode = CULL_MODE_NONE,
                                       .depth_op = COMPARE_OP_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .alpha_mode = ALPHA_MODE_MASK,
                                       .depth_test = true,
                                       .depth_write = true,
                                   },
                                   fwd_attachment);
            create_shader_material("forward-pass-transparent-mode", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass-transparent.frag.spv"},
                                   PipelineState{
                                       .cull_mode = CULL_MODE_BACK,
                                       .depth_op = COMPARE_OP_LESS_OR_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .alpha_mode = ALPHA_MODE_BLEND,
                                       .depth_test = true,
                                       .depth_write = false,
                                   },
                                   fwd_attachment);
            create_shader_material("forward-pass-transparent-mode", PASS_MODE_FORWARD,
                                   {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass-transparent.frag.spv"},
                                   PipelineState{
                                       .cull_mode = CULL_MODE_NONE,
                                       .depth_op = COMPARE_OP_LESS_OR_EQUAL,
                                       .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                       .alpha_mode = ALPHA_MODE_BLEND,
                                       .depth_test = true,
                                       .depth_write = false,
                                   },
                                   fwd_attachment);
        }
    }

} // namespace mirai