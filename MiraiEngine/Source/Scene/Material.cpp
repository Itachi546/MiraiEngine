#include "Material.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Shader.hpp"
namespace mirai {
    Material::Material(const std::string_view name) : name(name), dirty(true) {
        state = {};
    }

    EffectMaterial::EffectMaterial(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_infos) : Material(name) {
        shader = Shader::create_graphics_shader(name, shader_files, pipeline_state, attachment_infos);
        ASSERT(shader != nullptr);
    }

    void EffectMaterial::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    EffectMaterial::~EffectMaterial() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

    uint16_t custom_shader_material_id = 0;
    static uint16_t get_custom_shader_material_id() {
        return custom_shader_material_id++;
    }

    ShaderMaterial3D::ShaderMaterial3D(const std::string_view name,
                                       const std::vector<std::string> &shader_files,
                                       const PipelineState &pipeline_state,
                                       const PipelineAttachmentInfo &attachment_info)
        : Material3D(name) {
        // No shadow casting by default — no shadow-pass pipeline is registered.
        // Call add_render_flag(RENDER_FLAG_CAST_SHADOW) to opt in.
        shader = Shader::create_graphics_shader(std::string(name), shader_files, pipeline_state, attachment_info);
        custom_material_id = get_custom_shader_material_id();
        ASSERT(shader != nullptr);
    }

    ComputeShader::ComputeShader(const std::string &name, const std::string &shader_file) {
        shader = Shader::create_compute_shader(name, shader_file);
        ASSERT(shader != nullptr);
    }

    void ComputeShader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    ComputeShader::~ComputeShader() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

    RTShader::RTShader(const std::string &name, std::string ray_gen_shader_file, std::vector<std::string> ray_hit_shader_files, std::vector<std::string> ray_miss_shader_files, uint32_t max_recursion_depth) {
        shader = Shader::create_rt_shader(name, ray_gen_shader_file, ray_hit_shader_files, ray_miss_shader_files, max_recursion_depth);
        ASSERT(shader != nullptr);
    }

    void RTShader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(shader->pipeline_id);
    }

    RTShader::~RTShader() {
        if (shader)
            RenderingDevice::get()->destroy_pipelines(&shader->pipeline_id, 1);
    }

} // namespace mirai