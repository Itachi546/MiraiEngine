#include "Shader.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
/*
#include "Common/Hash.hpp"
#include "FrameGraph.hpp"

namespace mirai {
    ShaderMaterial::ShaderMaterial(const std::string &name) : name(name),
                                                              pipeline{K_INVALID_ID} {
        update_shader_material_id();
    }

    ShaderMaterial::~ShaderMaterial() {
        if (pipeline.is_valid())
            RenderingDevice::get()->destroy_pipelines(&pipeline, 1);
    }

    void ShaderMaterial::create_from_file(const std::vector<std::string> &shader_files, const ShaderMaterialProperties &properties) {

        this->shader_files = shader_files;
        this->properties = properties;
        update_shader_material_id();
    }

    void ShaderMaterial::bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass) {
        if (!pipeline.is_valid()) {
            pipeline = create_pipeline(renderpass);
        }
        command_buffer->bind_pipeline(pipeline);
        if (custom_uniform_sets.size() > 0) {
            command_buffer->set_uniform_sets(pipeline, custom_uniform_sets.data(), cast_u32(custom_uniform_sets.size()));
        }
    }

    PipelineID ShaderMaterial::create_pipeline(const FrameGraphRenderpassInfo *renderpass) {
    }

    void ShaderMaterial::update_shader_material_id() {
        utils::hash_combine(shader_material_id,
                            utils::djb2_hash_string(name),
                            properties.cull_mode,
                            properties.front_face,
                            properties.depth_clamp,
                            properties.depth_test,
                            properties.depth_write,
                            properties.blend,
                            properties.depth_op,
                            properties.topology,
                            properties.polygon_mode);
    }

    ComputeShader::ComputeShader(const std::string &name) : name(name) {
    }

    void ComputeShader::create_from_file(const std::string &file) {
        ShaderID shader = rendering_utils::create_shader_module_from_file(file);
        create_pipeline(shader);
        RenderingDevice::get()->destroy_shaders(&shader, 1);
    }

    void ComputeShader::bind(CommandBuffer *command_buffer) {
        ASSERT(pipeline.is_valid());
        command_buffer->bind_pipeline(pipeline);

        if (custom_uniform_sets.size() > 0) {
            command_buffer->set_uniform_sets(pipeline, custom_uniform_sets.data(), cast_u32(custom_uniform_sets.size()));
        }
    }

    ComputeShader::~ComputeShader() {
        RenderingDevice::get()->destroy_pipelines(&pipeline, 1);
    }

    void ComputeShader::create_pipeline(ShaderID shader) {
        RenderingDevice *device = RenderingDevice::get();
        pipeline = device->create_compute_pipeline(shader, name);
    }

} // namespace mirai
*/

namespace mirai {
    void Shader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(pipeline_id);
        if (bindings.size() > 0)
            command_buffer->set_uniform_sets(pipeline_id, bindings.data(), cast_u32(bindings.size()));
    }

    void MaterialShader::create_from_file(const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, const std::vector<std::string> &shader_files) {
        pipeline_id = PipelineHashMap::get()->add_or_get_graphics_pipeline(pipeline_state, attachment_info, shader_files, name);
    }
    void ComputeShader::create_from_file(const std::string &file) {
        pipeline_id = PipelineHashMap::get()->add_or_get_compute_pipeline(file, name);
    }
} // namespace mirai
