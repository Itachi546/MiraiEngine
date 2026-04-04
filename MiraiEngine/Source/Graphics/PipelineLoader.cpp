#include "PipelineLoader.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Engine/Log.hpp"
#include "Scene/Material.hpp"

namespace mirai {
    // GraphicsMaterial
    /*
    std::shared_ptr<Material> create_shader_material(const std::string &name, uint32_t shader_id, const std::vector<std::string> &shaders, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info) {
        std::shared_ptr<Shader> shader = Shader::create_from_file(name, shaders, pipeline_state, attachment_info);

        std::shared_ptr<Material> material = std::make_shared<Material>(name, shader_id);
        material->update_from_pipeline_state(pipeline_state);

        MaterialShaderLookup::get()->add_material_shader(material->get_hash(), shader);
        return material;
    }
    */
    void preload_shaders(ShaderRegistryMap *shader_registry_map) {
        // std::shared_ptr<Material> depth_prepass = create_shader_material("depth_prepass", SHADER_ID_DEPTH_PREPASS, {"SPIRV/depth-prepass.vert.spv"}, {.draw_mode = DRAWMODE_INDEXED_INDIRECT, .depth_test = true, .depth_write = true}, {.has_depth_attachment = true, .depth_attachment_format = FORMAT_D32_SFLOAT});
    }

} // namespace mirai