#include "Shader.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

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
