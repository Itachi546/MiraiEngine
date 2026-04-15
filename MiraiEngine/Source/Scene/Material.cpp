#include "Material.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Shader.hpp"
namespace mirai {
    Material::Material(const std::string_view name) : name(name) {
        material_state = {};
    }

    EffectMaterial::EffectMaterial(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_infos) : Material(name) {
        shader = Shader::create_from_file(name, shader_files, pipeline_state, attachment_infos);
        ASSERT(shader != nullptr);
    }

    void EffectMaterial::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    EffectMaterial::~EffectMaterial() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

    ShaderMaterial3D::ShaderMaterial3D(const std::string_view name,
                                       const std::vector<std::string> &shader_files,
                                       const PipelineState &pipeline_state,
                                       const PipelineAttachmentInfo &attachment_info)
        : Material3D(name) {
        // No shadow casting by default — no shadow-pass pipeline is registered.
        // Call add_render_flag(RENDER_FLAG_CAST_SHADOW) to opt in.
        material_state.render_flags = RENDER_FLAG_NONE;
        shader = Shader::create_from_file(std::string(name), shader_files, pipeline_state, attachment_info);
        ASSERT(shader != nullptr);
    }

    ShaderMaterial3D::~ShaderMaterial3D() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

    ComputeShader::ComputeShader(const std::string &name, const std::string &shader_file) {
        shader = Shader::create_from_file(name, shader_file);
        ASSERT(shader != nullptr);
    }

    void ComputeShader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    ComputeShader::~ComputeShader() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

} // namespace mirai