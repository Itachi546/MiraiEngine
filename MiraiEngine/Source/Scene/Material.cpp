#include "Material.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Shader.hpp"
namespace mirai {
    Material::Material(const std::string_view name) : name(name) {
        pipeline_state = {};
        current_key = pipeline_state.get_hash();
    }

    void Material::on_change_material() {
        uint32_t new_key = pipeline_state.get_hash();
        if (new_key == current_key)
            return;
        current_key = new_key;
    }

    void Material::update_from_pipeline_state(const PipelineState &pipeline_state) {
        this->pipeline_state = pipeline_state;
        on_change_material();
    }

    ShaderMaterial::ShaderMaterial(const std::string& name) : Material(name) {
    }

    ShaderMaterial::~ShaderMaterial() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

    ComputeShader::ComputeShader(const std::string& name, const std::string &shader_file) {
        shader = Shader::create_from_file(name, shader_file);
    }

    void ComputeShader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    ComputeShader::~ComputeShader() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

} // namespace mirai