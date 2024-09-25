#include "FrameGraph.hpp"

namespace mirai
{
    FrameGraphBuilder::FrameGraphBuilder() : resource_pool_nodes(64, "frame_graph_node")
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
        }

        ASSERT(node_description.outputs.size() > 0);
        uint32_t width = node_description.outputs[0].width;
        uint32_t height = node_description.outputs[0].height;
        for (uint32_t i = 0; i < node_description.outputs.size(); ++i)
        {
            ASSERT(width == node_description.outputs[i].width);
            ASSERT(height == node_description.outputs[i].height);

            const FrameGraphResourceOutput *output = &node_description.outputs[i];
            const FrameGraphResourceType &resource_type = output->resource_type;

            AttachmentInfo attachment_info;
            attachment_info.attachment_type = resource_type == FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN ? ATTACHMENT_TYPE_SWAPCHAIN : ATTACHMENT_TYPE_IMAGE;
            attachment_info.format = output->format;
            attachment_info.load_op = output->load_op;
            attachment_info.clear_color = output->clear_color;

            if (is_depth_format(attachment_info.format))
                node->render_pass.depth_attachment = std::move(attachment_info);
            else
                node->render_pass.color_attachments.push_back(std::move(attachment_info));
        }

        node->renderer = node_description.renderer;
        node->render_pass.width = width;
        node->render_pass.height = height;
        node->render_pass.depth = 1;

        resource_pool_maps.insert(std::make_pair(utils::djb2_hash_string(node->name), node_index));

        return FrameGraphNodeHandle{node_index};
    }

    FrameGraphBuilder::~FrameGraphBuilder()
    {
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