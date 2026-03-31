#include "Shader.hpp"
#include "ShaderHashMap.hpp"
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
        if (bindings.size() > 0)
            command_buffer->set_uniform_sets(pipeline_id, bindings.data(), cast_u32(bindings.size()));
    }
    /*
    void MaterialShader::create_from_file(const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, const std::vector<std::string> &shader_files) {
        pipeline_id = PipelineHashMap::get()->add_or_get_graphics_pipeline(pipeline_state, attachment_info, shader_files, name);
    }
    void ComputeShader::create_from_file(const std::string &file) {
        pipeline_id = PipelineHashMap::get()->add_or_get_compute_pipeline(file, name);
    }
    */
    Shader *Shader::create_from_file(const PipelineState &pipeline_state, const PipelineAttachmentInfo &attachment_info, const std::vector<std::string> &shader_files, const std::string &name) {
        ShaderHashMap *shader_map = ShaderHashMap::get();
        uint64_t shader_key = pipeline_state.get_hash();
        Shader *result = shader_map->get(shader_key);
        if (result != nullptr) {
            Log::Fatal("Loading already loaded shader");
            return result;
        }

        RasterizationState rs = RasterizationState::create();

        PipelineState::PipelineRenderState render_state = pipeline_state.render_state;
        rs.cull_mode = CullMode(render_state.fields.cull_mode);
        rs.front_face = FrontFace(render_state.fields.front_face);
        rs.enable_depth_clamp = static_cast<bool>(render_state.fields.depth_clamp);
        rs.polygon_mode = PolygonMode(render_state.fields.polygon_mode);
        rs.enable_depth_bias = render_state.fields.depth_bias;

        BlendState bs = BlendState::create();
        if (render_state.fields.blend_mode > 0)
            bs.enable = true;

        ASSERT(shader_files.size() > 0);
        std::vector<ShaderProgram> shader_programs;
        for (uint32_t i = 0; i < shader_files.size(); ++i) {
            shader_programs.emplace_back(ShaderProgram{load_shader_binary(shader_files[i])});
        }

        PipelineDescription pipeline_description = {
            .shader_programs = shader_programs,
            .topology = Topology(render_state.fields.topology),
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
            ds.enable_depth_write = render_state.fields.depth_write;
            ds.enable_depth_test = render_state.fields.depth_test;
            ds.compare_op = CompareOp(render_state.fields.depth_op);
            pipeline_description.depth_attachment_format = attachment_info.depth_attachment_format;
        }
        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        std::shared_ptr<Shader> shader = std::make_shared<Shader>(name);
        shader->draw_mode = DrawMode(pipeline_state.render_state.fields.draw_mode);
        shader->pipeline_id = pipeline;
        if (shader_map->get(shader_key) != nullptr)
            Log::Fatal("Graphics Shader key already exists ", shader_key);
        shader_map->add(shader_key, shader);
        return shader.get();
    }

    Shader *Shader::create_from_file(const std::string &shader_file, const std::string &name) {
        ShaderHashMap *shader_map = ShaderHashMap::get();
        uint64_t key = utils::djb2_hash_string(name);
        Shader *result = shader_map->get(key);
        if (result != nullptr)
            return result;

        ShaderProgram compute_program = {
            .byte_code = load_shader_binary(shader_file),
        };

        PipelineID pipeline = RenderingDevice::get()->create_compute_pipeline(compute_program, name);

        std::shared_ptr<Shader> shader = std::make_shared<Shader>(name);
        shader->pipeline_id = pipeline;

        if (shader_map->get(key) != nullptr) {
            Log::Fatal("Compute Shader key already exists ", key);
        }

        shader_map->add(key, shader);

        return shader.get();
    }
} // namespace mirai
