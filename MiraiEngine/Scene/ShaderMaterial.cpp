#include "ShaderMaterial.hpp"
#include "Common/Hash.hpp"
#include "ShaderMaterialCache.hpp"
#include "FrameGraph.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    ShaderMaterial::ShaderMaterial(const std::string &name) : name(name),
                                                  cull_mode(CullMode::CULL_MODE_BACK),
                                                  front_face(FrontFace::FRONT_FACE_COUNTER_CLOCKWISE),
                                                  enable_depth_test(false),
                                                  enable_depth_write(false),
                                                  hash(0),
                                                  is_resource_updated(true),
                                                  pipeline{K_INVALID_ID}
    {
    }

    void ShaderMaterial::create_from_file(const std::vector<std::string> &shader_files)
    {
        uint32_t shader_count = static_cast<uint32_t>(shader_files.size());
        std::vector<ShaderID> shaders(shader_count);

        for (uint32_t i = 0; i < shader_count; ++i)
            shaders[i] = rendering_utils::create_shader_module_from_file(shader_files[i]);

        uint32_t shader_hash = utils::djb2_hash_string(name);
        ShaderMaterialCache::get()->register_shader(shader_hash, std::move(shaders));

        calculate_hash();
    }

    void ShaderMaterial::set_resource(const std::string &name, ID resource)
    {
        uint32_t hash = utils::djb2_hash_string(name);
        auto found = resources.find(utils::djb2_hash_string(name));
        if (found == resources.end())
        {
            resources[hash] = Resource{name, resource, true};
            is_resource_updated = true;
        }
        else
        {
            if (found->second.resource_id.id == resource.id)
                return;

            found->second.resource_id = resource;
            found->second.dirty = true;
            is_resource_updated = true;
        }
    }

    void ShaderMaterial::bind(CommandBuffer *command_buffer, const FrameGraphNode *node, FrameGraph *frame_graph)
    {
        if (!pipeline.is_valid())
        {
            pipeline = ShaderMaterialCache::get()->get_pipeline(hash);
            if (!pipeline.is_valid())
                pipeline = create_pipeline(node, frame_graph);
        }

        if (is_resource_updated)
        {
            for (auto &resource : resources)
            {
                if (resource.second.dirty)
                {
                    RenderingDevice::get()->pipeline_set_resources(resource.second.name, pipeline, resource.second.resource_id);
                    resource.second.dirty = false;
                }
            }
            is_resource_updated = false;
        }

        command_buffer->bind_pipeline(pipeline);
    }

    void ShaderMaterial::set_push_constant(CommandBuffer *command_buffer, ShaderStage shader_stage, uint32_t offset, uint32_t size, void *data)
    {
        PipelineID pipeline = ShaderMaterialCache::get()->get_pipeline(hash);
#ifdef _DEBUG
        if (!pipeline.is_valid())
        {
            Log::Error("Can't bind push constant before binding pipeline");
        }
#endif
        command_buffer->set_push_constant(pipeline, shader_stage, offset, size, data);
    }

    void ShaderMaterial::calculate_hash()
    {
        hash = 0;
        utils::hash_combine(hash, name, (int)cull_mode, (int)front_face, enable_depth_test, enable_depth_write);
    }

    PipelineID ShaderMaterial::create_pipeline(const FrameGraphNode *node, FrameGraph *frame_graph)
    {
        RasterizationState rs = RasterizationState::create();
        rs.cull_mode = cull_mode;
        rs.front_face = front_face;

        BlendState bs = BlendState::create();
        PipelineDescription pipeline_description;

        std::vector<ShaderID> shaders = ShaderMaterialCache::get()->get_shaders(utils::djb2_hash_string(name));
        ASSERT(shaders.size() > 0);

        pipeline_description.shader_count = static_cast<uint32_t>(shaders.size());
        pipeline_description.shaders = shaders.data();
        pipeline_description.rasterization_state = &rs;
        pipeline_description.blend_state = &bs;

        DepthState ds = DepthState::create();
        std::vector<Format> color_attachment_formats;

        const FrameGraphRenderingInfo *rendering_info = &node->rendering_info;
        for (uint32_t i = 0; i < rendering_info->attachment_info.size(); ++i)
        {
            const FrameGraphAttachmentInfo *attachment = &rendering_info->attachment_info[i];
            if (i == rendering_info->depth_attachment_index)
            {
                ds.enable_depth_write = enable_depth_write;
                ds.enable_depth_test = enable_depth_test;
                pipeline_description.depth_attachment_format = attachment->format;
            }
            else
            {
                FrameGraphResource *resource = frame_graph->get_resource(node->outputs[i]);
                if (resource->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
                    color_attachment_formats.push_back(FORMAT_B8G8R8A8_UNORM);
                else
                    color_attachment_formats.push_back(attachment->format);
            }
        }
        pipeline_description.depth_state = &ds;
        pipeline_description.color_attachment_count = static_cast<uint32_t>(color_attachment_formats.size());
        pipeline_description.color_attachment_formats = color_attachment_formats.data();

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, name);
        ShaderMaterialCache::get()->add_pipeline(hash, pipeline);
        return pipeline;
    }

} // namespace mirai