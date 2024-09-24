#include "Material.hpp"
#include "Common/Hash.hpp"
#include "MaterialCache.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    Material::Material(const std::string &name) : name(name),
                                                  cull_mode(CullMode::CULL_MODE_BACK),
                                                  front_face(FrontFace::FRONT_FACE_COUNTER_CLOCKWISE),
                                                  enable_depth_test(false),
                                                  enable_depth_write(false),
                                                  hash(0)
    {
    }

    void Material::create_from_file(const std::vector<std::string> &shader_files)
    {
        uint32_t shader_count = static_cast<uint32_t>(shader_files.size());
        std::vector<ShaderID> shaders(shader_count);

        for (uint32_t i = 0; i < shader_count; ++i)
            shaders[i] = rendering_utils::create_shader_module_from_file(shader_files[i]);

        uint32_t shader_hash = utils::djb2_hash_string(name);
        MaterialCache::get()->register_shader(shader_hash, std::move(shaders));

        calculate_hash();
    }

    void Material::bind(CommandBuffer *command_buffer, RenderPass *node)
    {
        PipelineID pipeline = MaterialCache::get()->get_pipeline(hash);
        if (!pipeline.is_valid())
        {
            pipeline = create_pipeline(node);
        }

        command_buffer->bind_pipeline(pipeline);
    }

    void Material::calculate_hash()
    {
        hash = 0;
        utils::hash_combine(hash, name, (int)cull_mode, (int)front_face, enable_depth_test, enable_depth_write);
    }

    PipelineID Material::create_pipeline(RenderPass *render_pass)
    {
        RasterizationState rs = RasterizationState::create();
        rs.cull_mode = cull_mode;
        rs.front_face = front_face;

        BlendState bs = BlendState::create();
        PipelineDescription pipeline_description;

        std::vector<ShaderID> shaders = MaterialCache::get()->get_shaders(utils::djb2_hash_string(name));
        ASSERT(shaders.size() > 0);

        pipeline_description.shader_count = static_cast<uint32_t>(shaders.size());
        pipeline_description.shaders = shaders.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.blend_state = &bs;

        uint32_t color_attachment_count = static_cast<uint32_t>(render_pass->color_attachments.size());
        std::vector<Format> color_attachment_formats(color_attachment_count);
        for (uint32_t i = 0; i < color_attachment_count; ++i)
        {
            if (render_pass->color_attachments[i].attachment_type == ATTACHMENT_TYPE_SWAPCHAIN)
                color_attachment_formats[i] = FORMAT_B8G8R8A8_UNORM;
            else
                color_attachment_formats[i] = render_pass->color_attachments[i].format;
        }
        pipeline_description.color_attachment_count = color_attachment_count;
        pipeline_description.color_attachment_formats = color_attachment_formats.data();
        DepthState ds = DepthState::create();

        if (render_pass->depth_attachment.has_value())
        {
            ds.enable_depth_write = enable_depth_write;
            ds.enable_depth_test = enable_depth_test;
        }
        pipeline_description.depth_state = &ds;

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);

        MaterialCache::get()->add_pipeline(hash, pipeline);
        return pipeline;
    }

} // namespace mirai