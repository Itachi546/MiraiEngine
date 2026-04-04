#include "Shader.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Common/FileUtils.hpp"
namespace mirai {
    std::vector<uint8_t> load_shader_binary(const std::string &filename) {
        std::vector<uint8_t> result = utils::read_file_binary(filename);
        if (result.size() == 0)
            Log::Error("Error loading file: ", filename);
        return result;
    }
    void Shader::bind(CommandBuffer *command_buffer) {
        command_buffer->bind_pipeline(pipeline_id);
    }

    std::shared_ptr<Shader> Shader::create_from_file(const std::string &name, const std::vector<std::string> &shader_files, const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info) {
        RasterizationState rs = RasterizationState::create();
        rs.cull_mode = pipeline_state.cull_mode;
        rs.front_face = pipeline_state.front_face;
        rs.enable_depth_clamp = pipeline_state.cull_mode;
        rs.polygon_mode = pipeline_state.polygon_mode;
        rs.enable_depth_bias = pipeline_state.depth_bias;
        rs.enable_depth_clamp = pipeline_state.depth_clamp;

        BlendState bs = BlendState::create();
        bs.enable = pipeline_state.alpha_mode == ALPHA_MODE_BLEND;
        ASSERT(shader_files.size() > 0);

        std::vector<ShaderProgram> shader_programs;
        for (uint32_t i = 0; i < shader_files.size(); ++i) {
            shader_programs.emplace_back(ShaderProgram{load_shader_binary(shader_files[i])});
        }

        PipelineDescription pipeline_description = {
            .shader_programs = shader_programs,
            .topology = Topology(pipeline_state.topology),
            .rasterization_state = &rs,
            .vertex_description = nullptr,
            .blend_state = &bs,
            .depth_attachment_format = FORMAT_UNDEFINED,
        };

        DepthState ds = DepthState::create();
        std::vector<Format> color_attachment_formats;

        if (attachment_info.color_attachments_format.size() > 0) {
            color_attachment_formats.insert(color_attachment_formats.end(),
                                            attachment_info.color_attachments_format.begin(),
                                            attachment_info.color_attachments_format.end());
        }

        if (attachment_info.has_depth_attachment) {
            ds.enable_depth_write = pipeline_state.depth_write;
            ds.enable_depth_test = pipeline_state.depth_test;
            ds.compare_op = CompareOp(pipeline_state.depth_op);
            pipeline_description.depth_attachment_format = attachment_info.depth_attachment_format;
        }
        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        std::shared_ptr<Shader> shader(new Shader{name});
        shader->shader_files = shader_files;
        shader->attachment_info = attachment_info;
        shader->draw_mode = DrawMode(pipeline_state.draw_mode);
        shader->pipeline_id = pipeline;
        return shader;
    }

    std::shared_ptr<Shader> Shader::create_from_file(const std::string &name, const std::string &shader_file) {
        ShaderProgram compute_program = {
            .byte_code = load_shader_binary(shader_file),
        };

        PipelineID pipeline = RenderingDevice::get()->create_compute_pipeline(compute_program, name);

        std::shared_ptr<Shader> shader(new Shader{name});
        shader->pipeline_id = pipeline;
        shader->shader_files.push_back(shader_file);
        shader->is_graphics_shader = false;
        return shader;
    }
} // namespace mirai
