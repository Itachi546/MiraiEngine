#include "PipelineLoader.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Engine/Log.hpp"
#include "Scene/Material.hpp"
#include "Scene/Component.hpp"

namespace mirai {
    // GraphicsMaterial
    void create_shader_material(const std::string &name, PassMode pass_mode, const std::vector<std::string> &shaders, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, MeshType mesh_type = MESH_TYPE_STATIC) {
        ShaderRegistry *registry = ShaderRegistryMap::get()->get_registry(pass_mode);
        if (registry == nullptr) {
            registry = ShaderRegistryMap::get()->add_registry(pass_mode, std::make_shared<ShaderRegistry>(name));
        }

        std::shared_ptr<Shader> shader = Shader::create_from_file(name, shaders, pipeline_state, attachment_info);
        registry->add(pipeline_state.get_hash() | mesh_type, shader);
    }
    void preload_shaders(ShaderRegistryMap *shader_registry_map) {
        create_shader_material("depth_prepass", PASS_MODE_DEPTH_PREPASS, {"SPIRV/depth-prepass.vert.spv"}, {.draw_mode = DRAWMODE_INDEXED_INDIRECT, .depth_test = true, .depth_write = true}, {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});
        create_shader_material("depth_prepass_double_sided", PASS_MODE_DEPTH_PREPASS, {"SPIRV/depth-prepass.vert.spv"}, {.cull_mode = CULL_MODE_NONE, .draw_mode = DRAWMODE_INDEXED_INDIRECT, .depth_test = true, .depth_write = true}, {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});
        create_shader_material("depth_prepass_alpha_mask", PASS_MODE_DEPTH_PREPASS, {"SPIRV/depth-prepass-alpha.vert.spv", "SPIRV/depth-prepass-alpha.frag.spv"}, {.cull_mode = CULL_MODE_NONE, .draw_mode = DRAWMODE_INDEXED_INDIRECT, .alpha_mode = ALPHA_MODE_MASK, .depth_test = true, .depth_write = true}, {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});
    }

} // namespace mirai