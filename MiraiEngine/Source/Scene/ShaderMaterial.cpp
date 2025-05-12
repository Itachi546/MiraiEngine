#include "ShaderMaterial.hpp"
#include "Common/Hash.hpp"
#include "FrameGraph.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    ShaderMaterial::ShaderMaterial(const std::string &name) : name(name),
                                                              pipeline{K_INVALID_ID} {
    }

    ShaderMaterial::~ShaderMaterial() {
        if(pipeline.is_valid())
        RenderingDevice::get()->destroy_pipelines(&pipeline, 1);
    }

    void ShaderMaterial::create_from_file(const std::vector<std::string> &shader_files, const ShaderMaterialProperties &properties) {

        this->shader_files = shader_files;
        this->properties = properties;
    }

    void ShaderMaterial::bind(CommandBuffer *command_buffer, const FrameGraphRenderpassInfo *renderpass) {
        if (!pipeline.is_valid()) {
            pipeline = create_pipeline(renderpass);
        }

        command_buffer->bind_pipeline(pipeline,
                                      uniform_sets.data(),
                                      static_cast<uint32_t>(uniform_sets.size()),
                                      push_constants.data(),
                                      static_cast<uint32_t>(push_constants.size()));
    }

    PipelineID ShaderMaterial::create_pipeline(const FrameGraphRenderpassInfo *renderpass) {
        RasterizationState rs = RasterizationState::create();
        rs.cull_mode = properties.cull_mode;
        rs.front_face = properties.front_face;
        rs.enable_depth_clamp = properties.depth_clamp;

        BlendState bs = BlendState::create();
        if (properties.blend)
            bs.enable = true;
        PipelineDescription pipeline_description;

        ASSERT(shader_files.size() > 0);
        std::vector<ShaderID> shader_modules;
        for (uint32_t i = 0; i < shader_files.size(); ++i) {
            ShaderID shader = rendering_utils::create_shader_module_from_file(shader_files[i]);
            shader_modules.push_back(shader);
        }

        pipeline_description.topology = properties.topology;
        pipeline_description.shader_count = static_cast<uint32_t>(shader_modules.size());
        pipeline_description.shaders = shader_modules.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.blend_state = &bs;

        DepthState ds = DepthState::create();
        std::vector<Format> color_attachment_formats;

        for (uint32_t i = 0; i < renderpass->attachment_info.size(); ++i) {
            const FrameGraphAttachmentInfo *attachment = &renderpass->attachment_info[i];
            if (i == renderpass->depth_attachment_index) {
                ds.enable_depth_write = properties.depth_write;
                ds.enable_depth_test = properties.depth_test;
                ds.compare_op = properties.depth_op;
                pipeline_description.depth_attachment_format = attachment->format;
            } else {
                color_attachment_formats.push_back(attachment->format);
            }
        }
        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        RenderingDevice::get()->destroy_shaders(shader_modules.data(), cast_u32(shader_modules.size()));

        return pipeline;
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
        command_buffer->bind_pipeline(pipeline,
                                      uniform_sets.data(),
                                      static_cast<uint32_t>(uniform_sets.size()),
                                      push_constants.data(),
                                      static_cast<uint32_t>(push_constants.size()));
    }

    ComputeShader::~ComputeShader() {
        RenderingDevice::get()->destroy_pipelines(&pipeline, 1);
    }

    void ComputeShader::create_pipeline(ShaderID shader) {
        RenderingDevice *device = RenderingDevice::get();
        pipeline = device->create_compute_pipeline(shader, name);
    }

} // namespace mirai