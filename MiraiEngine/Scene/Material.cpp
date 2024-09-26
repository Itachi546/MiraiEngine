#include "Material.hpp"
#include "Common/Hash.hpp"
#include "MaterialCache.hpp"
#include "FrameGraph.hpp"
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

    void Material::bind(CommandBuffer *command_buffer, FrameGraphNode *node, FrameGraph *frame_graph)
    {
        PipelineID pipeline = MaterialCache::get()->get_pipeline(hash);
        if (!pipeline.is_valid())
        {
            pipeline = create_pipeline(node, frame_graph);
        }

        command_buffer->bind_pipeline(pipeline);
    }

    void Material::calculate_hash()
    {
        hash = 0;
        utils::hash_combine(hash, name, (int)cull_mode, (int)front_face, enable_depth_test, enable_depth_write);
    }

    PipelineID Material::create_pipeline(FrameGraphNode *node, FrameGraph *frame_graph)
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

        DepthState ds = DepthState::create();
        pipeline_description.depth_state = &ds;

        std::vector<Format> color_attachment_formats;

        for (auto &resource_handle : node->outputs)
        {
            FrameGraphResource *resource = frame_graph->get_resource(resource_handle);
            if (resource->is_depth_texture)
            {
                ds.enable_depth_write = enable_depth_write;
                ds.enable_depth_test = enable_depth_test;
                pipeline_description.depth_attachment_format = resource->format;
            }
            else
            {
                if (resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
                    color_attachment_formats.push_back(FORMAT_B8G8R8A8_UNORM);
                else
                    color_attachment_formats.push_back(resource->format);
            }
        }
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);
        MaterialCache::get()->add_pipeline(hash, pipeline);
        return pipeline;
    }

} // namespace mirai