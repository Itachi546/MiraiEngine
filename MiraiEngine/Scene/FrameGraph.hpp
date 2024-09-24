#pragma once

#include "Graphics/RenderingDevice.hpp"

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
        RENDERPASS_RESOURCE_TYPE_SWAPCHAIN,
        RENDERPASS_RESOURCE_TYPE_ATTACHMENT,
        RENDERPASS_RESOURCE_TYPE_TEXTURE,
    };

    struct FrameGraphResourceOutput
    {
        std::string name;
        FrameGraphResourceType resource_type;
        uint32_t width, height;
        Format format;
        AttachmentLoadOp load_op;
    };

    struct FrameGraphResourceInput
    {
        std::string name;
        FrameGraphResourceType resource_type;
    };

    class FrameGraphRenderPass
    {
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

    struct FrameGraphNode
    {
        std::string name;
        bool enabled;

        RenderPass render_pass;
    };

    class FrameGraphBuilder
    {
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
            auto found = nodes.find(name);
            if (found != nodes.end())
            {
                return found->second.get();
            }
            return nullptr;
        }

      private:
        FrameGraphBuilder *builder;
        std::vector<FrameGraphNodeDescription> node_descriptions;
        std::unordered_map<std::string, std::shared_ptr<FrameGraphNode>> nodes;
    };
} // namespace mirai