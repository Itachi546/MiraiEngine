#include "FrameGraph.hpp"

namespace mirai
{
    FrameGraphBuilder::FrameGraphBuilder() : resource_pool_nodes(64, "frame_graph_node"),
                                             resource_pool_resources(512, "frame_graph_resources"),
                                             device(RenderingDevice::get())
    {
    }

    FrameGraphNodeHandle FrameGraphBuilder::create_node(const FrameGraphNodeDescription &node_description)
    {
        uint32_t node_index = resource_pool_nodes.obtain();
        FrameGraphNode *node = resource_pool_nodes.access(node_index);
        node->name = node_description.name;
        node->enabled = node_description.enabled;

        for (uint32_t i = 0; i < node_description.inputs.size(); ++i)
        {
            // auto found = resources_map.find(utils::djb2_hash_string(node_description.inputs[i].name));
            // ASSERT(found != resources_map.end());
            // node->inputs.push_back(FrameGraphResourceHandle{found->second});
            FrameGraphResourceHandle handle = create_node_input(&node_description.inputs[i]);
            node->inputs.push_back(handle);
        }

        ASSERT(node_description.outputs.size() > 0);
        uint32_t width = node_description.outputs[0].width;
        uint32_t height = node_description.outputs[0].height;

        FrameGraphRenderingInfo &rendering_info = node->rendering_info;
        for (uint32_t i = 0; i < node_description.outputs.size(); ++i)
        {
            ASSERT(width == node_description.outputs[i].width);
            ASSERT(height == node_description.outputs[i].height);

            const FrameGraphResourceOutput *output = &node_description.outputs[i];
            const FrameGraphResourceType &resource_type = output->resource_type;

            if(is_depth_format(output->format)) {
                rendering_info.depth_attachment_index = i;
                rendering_info.has_stencil_attachment = is_stencil_format(output->format);
            }

            rendering_info.attachment_info.push_back(FrameGraphAttachmentInfo{
                .clear_color = output->clear_color,
                .format = output->format,
                .load_op = LOAD_OP_CLEAR,
            });

            if (output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT || output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
                node->outputs.push_back(create_node_output(output));
        }

        rendering_info.width = width;
        rendering_info.height = height;
        node->renderer = node_description.renderer;

        nodes_maps.insert(std::make_pair(utils::djb2_hash_string(node->name), node_index));

        return FrameGraphNodeHandle{node_index};
    }

    FrameGraphResourceHandle FrameGraphBuilder::create_node_output(const FrameGraphResourceOutput *output)
    {
        // SamplerDescription sampler_desc = SamplerDescription::create();
        uint32_t handle = resource_pool_resources.obtain();
        FrameGraphResource *resource = resource_pool_resources.access(handle);
        resource->resource_type = output->resource_type;
        resource->texture = TextureID{K_INVALID_ID};

        if (output->resource_type == FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT)
        {
            TextureDescription desc = {
                .width = output->width,
                .height = output->height,
                .depth = 1,
                .mip_levels = 1,
                .array_layers = 1,
                .texture_type = TEXTURE_TYPE_2D,
                .format = output->format,
                .usage_flags = 0,
                .sampler_desc = nullptr,
            };

            SamplerDescription sampler = SamplerDescription::create();
            if (is_depth_format(output->format))
            {
                desc.usage_flags = TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT;
                if (is_stencil_format(output->format))
                    desc.usage_flags |= TEXTURE_USAGE_STENCIL_ATTACHMENT_BIT;
            }
            else
            {
                desc.sampler_desc = &sampler;
                desc.usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT;
            }

            TextureID texture = device->create_texture(&desc, output->name.c_str());
            resource->texture = texture;
        }

        resources_map.insert(std::make_pair(utils::djb2_hash_string(output->name), handle));
        return FrameGraphResourceHandle{handle};
    }

    FrameGraphResourceHandle FrameGraphBuilder::create_node_input(const FrameGraphResourceInput *input)
    {
        uint32_t handle = resource_pool_resources.obtain();
        FrameGraphResource *resource = resource_pool_resources.access(handle);
        resource->resource_type = input->resource_type;

        ASSERT(input->resource_type != FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT);
        if (input->resource_type == FRAMEGRAPH_RESOURCE_TYPE_TEXTURE)
        {
            auto found = resources_map.find(utils::djb2_hash_string(input->name));
            ASSERT(found != resources_map.end());
            resource->texture = resource_pool_resources.access(found->second)->texture;
        }

        return FrameGraphResourceHandle{handle};
    }

    FrameGraphBuilder::~FrameGraphBuilder()
    {
        for (auto &[key, val] : resources_map)
        {
            FrameGraphResource *resource = resource_pool_resources.access(val);
            if (resource->resource_type != FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN)
            {
                device->destroy_texture(&resource->texture, 1);
            }
        }
        resource_pool_resources.release_all();
        resource_pool_nodes.release_all();
    }

    // @TODO Create from JSON file as well
    FrameGraph::FrameGraph(FrameGraphBuilder *builder) : builder(builder)
    {
    }

    void FrameGraph::compile()
    {
        for (uint32_t i = 0; i < node_descriptions.size(); ++i)
        {
            node_handles.push_back(builder->create_node(node_descriptions[i]));
        }
    }

    void FrameGraph::render(CommandBuffer *command_buffer, Scene *scene)
    {
        for (auto handle : node_handles)
        {
            FrameGraphNode *node = builder->get_node(handle);
            node->renderer->render(command_buffer, this, scene);
        }
    }
} // namespace mirai