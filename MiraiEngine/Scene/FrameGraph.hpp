#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/ResourcePool.hpp"
#include "Common/Hash.hpp"

#include <vector>
#include <string>
#include <unordered_map>
#include <memory>

namespace mirai
{

    constexpr const char *COLOR_ATTACHMENT_OUTPUT_NAME = "main_color_attachment";
    constexpr const char *DEPTH_ATTACHMENT_OUTPUT_NAME = "main_depth_attachment";

    class CommandBuffer;
    class Scene;
    class FrameGraph;

    enum FrameGraphResourceType
    {
        FRAMEGRAPH_RESOURCE_TYPE_SWAPCHAIN,
        FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
        FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
    };

    struct FrameGraphResourceOutput
    {
        std::string name;
        FrameGraphResourceType resource_type;
        uint32_t width, height;
        Format format;
        AttachmentLoadOp load_op;
        Color clear_color;
    };

    struct FrameGraphResourceInput
    {
        std::string name;
        FrameGraphResourceType resource_type;
    };

    class FrameGraphRenderPass
    {
      public:
        virtual void update() {}

        virtual void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, Scene *scene) = 0;
    };

    struct FrameGraphNodeDescription
    {
        std::string name;
        bool enabled;
        std::vector<FrameGraphResourceInput> inputs;
        std::vector<FrameGraphResourceOutput> outputs;
        std::shared_ptr<FrameGraphRenderPass> renderer;
    };

    struct FrameGraphResource
    {
        FrameGraphResourceType resource_type;
        TextureID texture;
        AttachmentLoadOp load_op;
        uint32_t width, height, depth;
        Color clear_colors;
    };

    using FrameGraphResourceHandle = uint32_t;

    struct FrameGraphNode
    {
        std::string name;
        bool enabled;

        std::vector<FrameGraphResourceHandle> inputs;
        std::vector<FrameGraphResourceHandle> outputs;
        RenderPass render_pass;

        std::shared_ptr<FrameGraphRenderPass> renderer;
    };
    using FrameGraphNodeHandle = uint32_t;

    class FrameGraphBuilder
    {
      public:
        FrameGraphBuilder();
        FrameGraphNodeHandle create_node(const FrameGraphNodeDescription &node_description);

        FrameGraphNode *get_node(FrameGraphNodeHandle handle)
        {
            return resource_pool_nodes.access(handle);
        }

        FrameGraphNode *get_node(const std::string &name)
        {
            auto found = resource_pool_maps.find(utils::djb2_hash_string(name));
            return resource_pool_nodes.access(found->second);
        }

        ~FrameGraphBuilder();

      private:
        ResourcePool<FrameGraphNode> resource_pool_nodes;
        std::unordered_map<uint32_t, uint32_t> resource_pool_maps;
    };

    class FrameGraph
    {
      public:
        FrameGraph(FrameGraphBuilder *builder);

        void compile();

        void render(CommandBuffer *command_buffer, Scene *scene);

        void add_node(const FrameGraphNodeDescription &node)
        {
            node_descriptions.push_back(node);
        }

        FrameGraphNode *get_node(const std::string &name)
        {
            return builder->get_node(name);
        }

      private:
        FrameGraphBuilder *builder;
        std::vector<FrameGraphNodeDescription> node_descriptions;
        std::vector<FrameGraphNodeHandle> node_handles;
    };
} // namespace mirai